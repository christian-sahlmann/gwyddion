/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * VTK structured grid file
 * .vtk
 * Export
 **/

#include "config.h"
#include <math.h>
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <app/gwyapp.h>

#include "err.h"

#define EXTENSION ".vtk"

static gboolean module_register(void);
static gint     vtk_detect     (const GwyFileDetectInfo *fileinfo,
                                gboolean only_name);
static gboolean vtk_export     (GwyContainer *data,
                                const gchar *filename,
                                GwyRunType mode,
                                GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as VTK structured grids."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("vtk",
                           N_("VTK structured grid (.vtk)"),
                           (GwyFileDetectFunc)&vtk_detect,
                           NULL,
                           NULL,
                           (GwyFileSaveFunc)&vtk_export);

    return TRUE;
}

static gint
vtk_detect(const GwyFileDetectInfo *fileinfo,
           G_GNUC_UNUSED gboolean only_name)
{
    return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 30 : 0;
}

static inline gint
print_with_decimal_dot(FILE *fh,
                       gchar *formatted_number,
                       const gchar *decimal_dot,
                       guint decimal_dot_len)
{
    gchar *pos = strstr(formatted_number, decimal_dot);

    if (!pos)
        return fputs(formatted_number, fh);
    else {
        pos[0] = '.';
        if (decimal_dot_len == 1)
            return fputs(formatted_number, fh);
        else {
            gint l1, l2;

            pos[1] = '\0';
            if ((l1 = fputs(formatted_number, fh)) == EOF)
                return EOF;
            if ((l2 = fputs(pos + decimal_dot_len, fh)) == EOF)
                return EOF;
            return l1 + l2;
        }
    }
}

static gboolean
vtk_export(G_GNUC_UNUSED GwyContainer *data,
           const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    const guchar *title = "Unknown channel";
    GwyDataField *dfield;
    guint xres, yres, i, j, decimal_dot_len;
    gint id;
    struct lconv *locale_data;
    const gchar *decimal_dot;
    gdouble min, max, q;
    gdouble *d;
    gchar buf[40];
    FILE *fh;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!dfield) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    locale_data = localeconv();
    decimal_dot = locale_data->decimal_point;
    g_return_val_if_fail(decimal_dot, FALSE);
    decimal_dot_len = strlen(decimal_dot);
    g_return_val_if_fail(decimal_dot_len, FALSE);

    if (!(fh = g_fopen(filename, "w"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    // Do not bother checking errors here.  If some write fails we will get
    // more errors below.
    fprintf(fh, "# vtk DataFile Version 2.0\n");
    g_snprintf(buf, sizeof(buf), "/%d/data/title", id);
    gwy_container_gis_string_by_name(data, buf, &title);
    fprintf(fh, "%s\n", title);
    fprintf(fh, "ASCII\n");
    fprintf(fh, "DATASET STRUCTURED_GRID\n");
    fprintf(fh, "DIMENSIONS %u %u 1\n", xres, yres);
    fprintf(fh, "POINTS %u float\n", xres*yres);

    d = gwy_data_field_get_data(dfield);
    gwy_data_field_get_min_max(dfield, &min, &max);
    q = (max == min) ? 0.0 : 0.2*sqrt(xres*yres)/(max - min);

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++, d++) {
            g_snprintf(buf, sizeof(buf), "%u %u %.6g\n", i, j, q*(*d - min));
            if (print_with_decimal_dot(fh, buf,
                                       decimal_dot, decimal_dot_len) == EOF)
                goto fail;
        }
    }
    fclose(fh);

    return TRUE;

fail:
    err_WRITE(error);
    fclose(fh);
    g_unlink(filename);

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
