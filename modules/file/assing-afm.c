/*
 *  @(#) $Id$
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

#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libgwyddion/gwywin32unistd.h>

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include "get.h"

#define EXTENSION ".afm"

#define Angstrom (1e-10)
#define Z_SCALE (0.5*Angstrom)

typedef struct {
    guint res;
    gdouble real;
    gdouble range;
} AFMFile;

static gboolean      module_register       (const gchar *name);
static gint          aafm_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* aafm_load             (const gchar *filename);
static gboolean      read_binary_data      (guint res,
                                            gdouble *data,
                                            const guchar *buffer);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Assing AFM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo aafm_func_info = {
        "assing_afm",
        N_("Assing AFM files (.afm)"),
        (GwyFileDetectFunc)&aafm_detect,
        (GwyFileLoadFunc)&aafm_load,
        NULL
    };

    gwy_file_func_register(name, &aafm_func_info);

    return TRUE;
}

static gint
aafm_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    gint score = 0;
    guint res;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 17 : 0;

    if (fileinfo->buffer_len >= 12
        && (res = ((guint)fileinfo->buffer[1] << 8 | fileinfo->buffer[0]))
        && fileinfo->file_size == 2*res*res + 10)
        score = 90;

    return score;
}

static GwyContainer*
aafm_load(const gchar *filename)
{
    GwySIUnit *unit;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    AFMFile afmfile;
    GwyDataField *dfield;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < 12) {
        g_warning("File %s is not a Createc image file", filename);
        gwy_file_abandon_contents(buffer, size, &err);
        g_clear_error(&err);
        return NULL;
    }

    p = buffer;
    afmfile.res = get_WORD(&p);
    afmfile.real = Angstrom*get_FLOAT(&p);
    if (size < afmfile.res * afmfile.res + 10) {
        g_warning("Truncated file `%s'", filename);
        gwy_file_abandon_contents(buffer, size, &err);
        g_clear_error(&err);
        return NULL;
    }

    dfield = gwy_data_field_new(afmfile.res, afmfile.res,
                                afmfile.real, afmfile.real,
                                FALSE);
    read_binary_data(afmfile.res, gwy_data_field_get_data(dfield), p);
    p += 2*afmfile.res*afmfile.res;
    afmfile.range = get_FLOAT(&p);
    gwy_data_field_multiply(dfield, Z_SCALE);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_duplicate(unit);
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);

    gwy_file_abandon_contents(buffer, size, &err);
    g_clear_error(&err);

    return container;
}

static gboolean
read_binary_data(guint res,
                 gdouble *data,
                 const guchar *buffer)
{
    guint i, j;
    guint16 *p = (gint16*)buffer;

    for (i = 0; i < res*res; i++) {
        j = (res - 1 - (i % res))*res + i/res;
        data[j] = GINT16_FROM_LE(p[i]);
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
