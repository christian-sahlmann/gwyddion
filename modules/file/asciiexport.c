/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <errno.h>
#include <string.h>

#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>

#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define EXTENSION ".txt"

static gboolean      module_register     (const gchar *name);
static gint          asciiexport_detect  (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static gboolean      asciiexport_export  (GwyContainer *data,
                                          const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Exports data as simple ASCII matrix."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo asciiexport_func_info = {
        "asciiexport",
        N_("ASCII data matrix (.txt)"),
        (GwyFileDetectFunc)&asciiexport_detect,
        NULL,
        NULL,
        (GwyFileSaveFunc)&asciiexport_export,
    };

    gwy_file_func_register(name, &asciiexport_func_info);

    return TRUE;
}

static gint
asciiexport_detect(const GwyFileDetectInfo *fileinfo,
                   G_GNUC_UNUSED gboolean only_name)
{
    return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;
}

static gboolean
asciiexport_export(GwyContainer *data,
                   const gchar *filename,
                   G_GNUC_UNUSED GwyRunType mode,
                   GError **error)
{
    GwyDataField *dfield;
    gint xres, yres, i;
    gdouble *d;
    FILE *fh;

    if (!(fh = g_fopen(filename, "w"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot open file for writing: %s"), g_strerror(errno));
        return FALSE;
    }

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++) {
        if (fprintf(fh, "%g%c", d[i],
                    (i + 1) % xres ? '\t' : '\n') < 2) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                        _("Cannot write to file: %s"), g_strerror(errno));
            fclose(fh);
            g_unlink(filename);
            return FALSE;
        }
    }
    fclose(fh);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
