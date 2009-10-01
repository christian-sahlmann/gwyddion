/*
 *  $Id: nanotop.c, v 1.2 2006/04/06 16:32:48
 *  Copyright (C) 2006 Alexander Kovalev, Metal-Polymer Research Institute
 *  E-mail: av_kov@tut.by
 *
 *  Partially based on apefile.c,
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanotop-spm">
 *   <comment>Nanotop SPM data</comment>
 *   <glob pattern="*.spm"/>
 *   <glob pattern="*.SPM"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanotop SPM
 * .spm
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define EXTENSION ".spm"
#define nanometer (1e-9)

enum {
    PING_SIZE = 4*sizeof(guint16),
    HEADER_SIZE = 512,
};

/* =============== header of SPM file======================================= */
typedef struct {
  /* number of points for one step and full X-size in points; tx - not used */
  guint16 tx, mx;
  /* number of points for one step and full Y-size in points; ty - not used */
  guint16 ty, my;
  /* scale factor for X,Y and Z axes (nm/point)                             */
  gdouble Kx, Ky, Kz;
  /* label of z-axis                                                        */
  gchar ZUnit[6];
  /* label of scanning plane                                                */
  gchar XYUnit[6];
  /* min of data                                                            */
  guint16 min;
  /* max of data                                                            */
  guint16 max;
  /* time of scanning line                                                  */
  guint16 timeline;
  /* date of creation data                                                  */
  gchar date[8];
  /* time of creation data                                                  */
  gchar time[5];
  /* notes                                                                  */
  gchar note[301];
  /* reserved                                                               */
  gchar void_field[94];
  /* version of SPM "Nanotop"                                               */
  gchar Version[64];
} SPMFile;
/* it's consists of 512 bytes                                             */
/* ========================================================================= */


static gboolean      module_register   (void);
static gint          nanotop_detect    (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* nanotop_load      (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* read_data_field   (SPMFile *spmfile,
                                        const guchar *ptr);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NANOTOP AFM files"),
    "Alexander Kovalev <av_kov@tut.by>",
    "1.8",
    "Alexander Kovalev, Metal-Polymer Research Institute",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanotop",
                           N_("Nanotop files (.spm)"),
                           (GwyFileDetectFunc)&nanotop_detect,
                           (GwyFileLoadFunc)&nanotop_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanotop_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;
    gsize expected = 0;
    guint xres, yres;
    const guchar *p;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len < PING_SIZE)
        return 0;

    p = fileinfo->head;
    gwy_get_guint16_le(&p);
    xres = gwy_get_guint16_le(&p);
    gwy_get_guint16_le(&p);
    yres = gwy_get_guint16_le(&p);

    expected = 2*xres*yres + HEADER_SIZE; /* expected file size */
    if (expected == fileinfo->file_size)
        score = 100;      /* that is fine! */

    return score;
}


static GwyContainer*
nanotop_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    SPMFile spmfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    /* read file header */
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;
    spmfile.tx = gwy_get_guint16_le(&p);
    spmfile.mx = gwy_get_guint16_le(&p);
    spmfile.ty = gwy_get_guint16_le(&p);
    spmfile.my = gwy_get_guint16_le(&p);

    if (err_DIMENSION(error, spmfile.mx) || err_DIMENSION(error, spmfile.my)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (err_SIZE_MISMATCH(error, HEADER_SIZE + 2*spmfile.mx*spmfile.my, size,
                          TRUE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    spmfile.Kx = gwy_get_gfloat_le(&p);
    spmfile.Ky = gwy_get_gfloat_le(&p);
    spmfile.Kz = gwy_get_gfloat_le(&p);
    get_CHARARRAY0(spmfile.ZUnit, &p);
    get_CHARARRAY0(spmfile.XYUnit, &p);
    spmfile.min = gwy_get_guint16_le(&p);
    spmfile.max = gwy_get_guint16_le(&p);
    spmfile.timeline = gwy_get_guint16_le(&p);
    get_CHARARRAY(spmfile.date, &p);
    get_CHARARRAY(spmfile.time, &p);
    get_CHARARRAY(spmfile.note, &p);
    get_CHARARRAY(spmfile.void_field, &p);
    get_CHARARRAY(spmfile.Version, &p);

    /* read data from buffer */
    dfield = read_data_field(&spmfile, p + 2);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        gwy_app_channel_title_fall_back(container, 0);
    }

    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static GwyDataField*
read_data_field(SPMFile *spmfile, const guchar *ptr)
{
    GwyDataField *dfield;
    GwySIUnit *unit = NULL;
    gdouble xreal, yreal;
    gdouble *data;
    gint i, n;
    const guint16 *p;

    xreal = spmfile->mx*spmfile->Kx;
    yreal = spmfile->my*spmfile->Ky;
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    dfield = gwy_data_field_new(spmfile->mx, spmfile->my,
                                xreal*nanometer, yreal*nanometer,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    p = (const guint16*)ptr;

    n = spmfile->mx * spmfile->my;

    for (i = 0; i < n; i++)
        *(data++) = GINT16_FROM_LE(*(p++));

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    if (strcmp(spmfile->ZUnit, "deg") != 0) {
      gwy_data_field_multiply(dfield, spmfile->Kz*nanometer);
      unit = gwy_si_unit_new("m");
    }
    else {
      gwy_data_field_multiply(dfield, spmfile->Kz);
      unit = gwy_si_unit_new("deg");
    }
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
