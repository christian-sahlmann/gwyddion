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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define EXTENSION ".txt"

static gboolean      module_register     (const gchar *name);
static gint          asciiexport_detect  (const gchar *filename,
                                          gboolean only_name);
static gboolean      asciiexport_save    (GwyContainer *data,
                                          const gchar *filename);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "asciiexport",
    "Load and save Gwyddion native serialized objects.",
    "Yeti <yeti@gwyddion.net>",
    "0.1",
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
        "ASCII data matrix (" EXTENSION ")",
        (GwyFileDetectFunc)&asciiexport_detect,
        NULL,
        (GwyFileSaveFunc)&asciiexport_save,
    };

    gwy_file_func_register(name, &asciiexport_func_info);

    return TRUE;
}

static gint
asciiexport_detect(const gchar *filename,
                   G_GNUC_UNUSED gboolean only_name)
{
    return g_str_has_suffix(filename, EXTENSION) ? 20 : 0;
}

static gboolean
asciiexport_save(GwyContainer *data,
                 const gchar *filename)
{
    GwyDataField *dfield;
    gint xres, yres, i, j;
    gdouble *d;
    FILE *fh;

    if (!(fh = fopen(filename, "w")))
        return FALSE;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            if (fprintf(fh, "%g%c", d[i*xres + j],
                        j == xres-1 ? '\n' : '\t') < 2) {
                unlink(filename);
                return FALSE;
            }
        }
    }
    fclose(fh);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
