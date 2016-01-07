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
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nano-measuring-machine-spm">
 *   <comment>Nano Measuring Machine data header</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="------------------------------------------"/>
 *     <match type="string" offset="44" value="Scan procedure description file"/>
 *   </magic>
 *   <glob pattern="*.dsc"/>
 *   <glob pattern="*.DSC"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Nano Measuring Machine profiles header
 * # Usually accompanied with unidentifiable data files.
 * 0 string ------------------------------------------
 * >44 string Scan\ procedure\ description\ file Nano Measuring Machine data header
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nano Measuring Machine profile data
 * *.dsc + *.dat
 * Read[1]
 * [1] XYZ data are interpolated to a regular grid upon import.
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
    guint nfiles;
    guint blocksize;
    gulong ndata;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} NMMXYZInfo;

typedef struct {
    guint nchannels;
    gboolean *include_channel;
    gint xres;
    gint yres;
} NMMXYZArgs;

typedef struct {
    NMMXYZArgs *args;
    GtkWidget *dialog;
    GtkWidget *info;
    GtkWidget **include_channel;
} NMMXYZControls;

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
static void          gather_data_files         (const gchar *filename,
                                                NMMXYZInfo *info,
                                                GArray *dscs,
                                                GArray *data);
static PointXYZ*     create_points_with_xy     (GArray *data,
                                                guint blocksize);
static void          create_data_field         (GwyContainer *container,
                                                const NMMXYZInfo *info,
                                                const NMMXYZArgs *args,
                                                GArray *dscs,
                                                GArray *data,
                                                PointXYZ *points,
                                                guint i);
static void          find_data_range           (const PointXYZ *points,
                                                NMMXYZInfo *info);
static void          read_data_file            (GArray *data,
                                                const gchar *filename,
                                                guint nrec);
static gboolean      profile_descriptions_match(GArray *descs1,
                                                GArray *descs2);
static void          read_profile_description  (const gchar *filename,
                                                GArray *dscs);
static void          free_profile_descriptions (GArray *dscs,
                                                gboolean free_array);
static void          copy_profile_descriptions (GArray *source,
                                                GArray *dest);

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
    NMMXYZInfo info;
    NMMXYZArgs args;
    GwyContainer *container = NULL;
    GArray *data = NULL;
    GArray *dscs = NULL;
    PointXYZ *points = NULL;
    guint i;
    gdouble q;

    /* In principle we can load data non-interactively, but it is going to
     * take several minutes which is not good for previews... */
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Nano Measuring Machine data import "
                      "must be run as interactive."));
        return NULL;
    }

    dscs = g_array_new(FALSE, FALSE, sizeof(NMMXYZProfileDescription));
    gather_data_files(filename, &info, dscs, NULL);
    if (!info.ndata) {
        err_NO_DATA(error);
        goto fail;
    }

    /* TODO: here goes the dialogue. */

    free_profile_descriptions(dscs, FALSE);
    data = g_array_new(FALSE, FALSE, sizeof(gdouble));
    gather_data_files(filename, &info, dscs, data);

    points = create_points_with_xy(data, info.blocksize);
    find_data_range(points, &info);

    /* XXX: Needs some safeguards.  But anyway, this will be selectable in the
     * GUI when we have it. */
    q = sqrt((info.xmax - info.xmin)/(info.ymax - info.ymin));
    args.xres = (gint)ceil(2048.0*q);
    args.yres = (gint)ceil(2048.0/q);

    container = gwy_container_new();
    for (i = 0; i < info.blocksize-2; i++) {
        create_data_field(container, &info, &args, dscs, data, points, i);
    }

fail:
    g_free(points);
    free_profile_descriptions(dscs, TRUE);
    if (data)
        g_array_free(data, TRUE);

    return container;
}

static PointXYZ*
create_points_with_xy(GArray *data, guint blocksize)
{
    PointXYZ *points;
    const gdouble *d;
    gulong i, npts;

    gwy_debug("data->len %u, block size %u", data->len, blocksize);
    d = (const gdouble*)data->data;
    npts = data->len/blocksize;
    points = g_new(PointXYZ, npts);
    gwy_debug("creating %lu XYZ points", npts);
    for (i = 0; i < npts; i++) {
        points[i].x = d[0];
        points[i].y = d[1];
        d += blocksize;
    }

    return points;
}

static void
find_data_range(const PointXYZ *points, NMMXYZInfo *info)
{
    gdouble xmin = G_MAXDOUBLE;
    gdouble ymin = G_MAXDOUBLE;
    gdouble xmax = -G_MAXDOUBLE;
    gdouble ymax = -G_MAXDOUBLE;
    guint i;

    gwy_debug("finding the XY range from %lu records", info->ndata);
    for (i = 0; i < info->ndata; i++) {
        const gdouble x = points[i].x, y = points[i].y;
        if (x < xmin)
            xmin = x;
        if (x > xmax)
            xmax = x;
        if (y < ymin)
            ymin = y;
        if (y > ymax)
            ymax = y;
    }

    info->xmin = xmin;
    info->xmax = xmax;
    info->ymin = ymin;
    info->ymax = ymax;
    gwy_debug("full data range [%g,%g]x[%g,%g]", xmin, xmax, ymin, ymax);
}

static void
create_data_field(GwyContainer *container,
                  const NMMXYZInfo *info,
                  const NMMXYZArgs *args,
                  GArray *dscs,
                  GArray *data,
                  PointXYZ *points,
                  guint i)
{
    const NMMXYZProfileDescription *dsc
        = &g_array_index(dscs, NMMXYZProfileDescription, i + 2);
    GwyDataField *dfield;
    gulong k, ndata;
    guint blocksize;
    const gdouble *d;
    const gchar *zunit = NULL;
    GQuark quark;

    gwy_debug("regularising field #%u %s (%s)",
              i, dsc->short_name, dsc->long_name);
    dfield = gwy_data_field_new(args->xres, args->yres,
                                info->xmax - info->xmin,
                                info->ymax - info->ymin,
                                FALSE);

    if (gwy_stramong(dsc->short_name, "Lz", "Az", "-Lz+Az", "XY vector", NULL))
        zunit = "m";

    gwy_data_field_set_xoffset(dfield, info->xmin);
    gwy_data_field_set_yoffset(dfield, info->ymin);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), zunit);

    d = (const gdouble*)data->data + (i + 2);
    blocksize = info->blocksize;
    ndata = info->ndata;
    for (k = 0; k < ndata; k++) {
        points[k].z = *d;
        d += blocksize;
    }

    gwy_data_field_average_xyz(dfield, NULL, points, ndata);

    quark = gwy_app_get_data_key_for_id(i);
    gwy_container_set_object(container, quark, dfield);
    g_object_unref(dfield);

    quark = gwy_app_get_data_title_key_for_id(i);
    gwy_container_set_const_string(container, quark, dsc->long_name);
}

/* Fills nfiles, blocksize and ndata fields of @info.  When @data is %NULL
 * the number of files and data is taken from headers.
 *
 * If non-NULL @data array is passed the raw data are immediately loaded.
 *
 * TODO: We need a progress bar.
 * TODO: If include_channel is passed we can both save memory (by skipping
 *       unwanted raw data) and time (by moving forward to the start of next
 *       whitespace instead of parsing the coordinates).
 */
static void
gather_data_files(const gchar *filename, NMMXYZInfo *info,
                  GArray *dscs, GArray *data)
{
    const NMMXYZProfileDescription *dsc;
    GArray *this_dscs = NULL;
    GDir *dir = NULL;
    const gchar *fname;
    gchar *dirname = NULL, *basename = NULL, *s;
    GString *str;

    info->nfiles = 0;
    info->ndata = 0;
    info->blocksize = 0;

    dirname = g_path_get_dirname(filename);
    basename = g_path_get_basename(filename);
    str = g_string_new(basename);
    g_free(basename);
    if ((s = strrchr(str->str, '.'))) {
        g_string_truncate(str, s - str->str);
        g_string_append(str, "_");
    }
    basename = g_strdup(str->str);

    gwy_debug("scanning dir <%s> for files <%s*%s>",
              dirname, basename, EXTENSION);
    dir = g_dir_open(dirname, 0, NULL);
    if (!dir)
        goto fail;

    this_dscs = g_array_new(FALSE, FALSE, sizeof(NMMXYZProfileDescription));
    while ((fname = g_dir_read_name(dir))) {
        gwy_debug("candidate file %s", fname);
        if (!g_str_has_prefix(fname, basename)
            || !g_str_has_suffix(fname, EXTENSION))
            continue;

        s = g_build_filename(dirname, fname, NULL);
        free_profile_descriptions(this_dscs, FALSE);
        read_profile_description(s, this_dscs);
        g_free(s);

        gwy_debug("found DSC file %s (%u records)", fname, this_dscs->len);
        if (!this_dscs->len)
            continue;

        if (!info->nfiles) {
            copy_profile_descriptions(this_dscs, dscs);
            info->blocksize = this_dscs->len;
        }
        else {
            if (!profile_descriptions_match(dscs, this_dscs)) {
                gwy_debug("non-matching profile descriptions for %s", fname);
                continue;
            }
        }

        info->nfiles++;

        if (data) {
            s = g_build_filename(dirname, fname, NULL);
            g_string_assign(str, s);
            g_free(s);

            if ((s = strrchr(str->str, '.'))) {
                g_string_truncate(str, s - str->str);
                g_string_append(str, ".dat");
            }
            read_data_file(data, str->str, info->blocksize);
            info->ndata = data->len/info->blocksize;
        }
        else {
            dsc = &g_array_index(this_dscs, NMMXYZProfileDescription, 0);
            info->ndata += dsc->npts;
        }
    }

fail:
    free_profile_descriptions(this_dscs, TRUE);
    if (dir)
        g_dir_close(dir);
    g_string_free(str, TRUE);
    g_free(basename);
    g_free(dirname);
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
                  descs1->len, descs2->len);
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

static void
read_profile_description(const gchar *filename, GArray *dscs)
{
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

    g_assert(!dscs->len);
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
        if (g_array_index(dscs, NMMXYZProfileDescription, i).id != i) {
            gwy_debug("non-matching channel id #%u", i);
            goto fail;
        }
    }

    /* We cannot read files with different number of points for each channel. */
    for (i = 1; i < dscs->len; i++) {
        if (g_array_index(dscs, NMMXYZProfileDescription, i).npts
            != g_array_index(dscs, NMMXYZProfileDescription, i-1).npts) {
            gwy_debug("non-matching number of points per channel #%u", i);
            goto fail;
        }
    }

fail:
    g_free(buffer);
    if (pieces)
        g_strfreev(pieces);
}

static void
free_profile_descriptions(GArray *dscs, gboolean free_array)
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

    if (free_array)
        g_array_free(dscs, TRUE);
    else
        g_array_set_size(dscs, 0);
}

static void
copy_profile_descriptions(GArray *source, GArray *dest)
{
    guint i;

    g_assert(source);
    g_assert(dest);
    g_assert(!dest->len);

    for (i = 0; i < source->len; i++) {
        NMMXYZProfileDescription dsc
            = g_array_index(source, NMMXYZProfileDescription, i);

        dsc.date = g_strdup(dsc.date);
        dsc.short_name = g_strdup(dsc.short_name);
        dsc.long_name = g_strdup(dsc.long_name);
        g_array_append_val(dest, dsc);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
