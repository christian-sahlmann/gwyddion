/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

/* See:
 * http://www.weizmann.ac.il/Chemical_Research_Support/surflab/peter/headers/burl.html
 * http://www.physik.uni-freiburg.de/~doerr/readimg
 */
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".img"

#define HEADER_SIZE_MIN 42

#define Angstrom (1e-10)

typedef struct {
    gdouble version;
    guint32 header_size;
    guint32 xres;
    guint32 yres;
    guint32 xrangemax;
    guint32 yrangemax;
    guint32 zrangemax;
    guint32 xrange;
    guint32 yrange;
    guint32 zrange;
    guint32 data_type;
    guint32 afm_head_id;
    guint32 zoom_factor;
    guint32 z_gain;
    /* there are other fields but I have no idea how they depend on the version
     * so just ignore them */
} IMGFile;

static gboolean      module_register(void);
static gint          burleigh_detect(const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* burleigh_load  (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh IMG data files version 2.x."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("burleigh",
                           N_("Burleigh 2.x files (.img)"),
                           (GwyFileDetectFunc)&burleigh_detect,
                           (GwyFileLoadFunc)&burleigh_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
burleigh_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    const guchar *buffer;
    gdouble version;
    guint xres, yres, hsize;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < HEADER_SIZE_MIN + 2)
        return 0;

    buffer = fileinfo->buffer;
    version = get_FLOAT(&buffer);
    if (version < 0.0 || version >= 3.0)
        return 0;

    if (ROUND(10*version) == 21)
        hsize = 48;
    else
        hsize = get_WORD(&buffer);

    xres = get_WORD(&buffer);
    yres = get_WORD(&buffer);
    gwy_debug("version: %g", version);
    gwy_debug("header size: %u", hsize);
    gwy_debug("xres: %u", xres);
    gwy_debug("yres: %u", yres);
    gwy_debug("file size: %u", fileinfo->file_size);
    gwy_debug("expected size: %u", 2*xres*yres + hsize);
    if (hsize >= HEADER_SIZE_MIN
        && version < 3
        && fileinfo->file_size == 2*xres*yres + hsize)
        score = 95;

    return score;
}

static GwyContainer*
burleigh_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwySIUnit *unit;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    IMGFile imgfile;
    GwyDataField *dfield;
    gdouble *data;
    gint16 *d;
    gdouble scale;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE_MIN + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;
    imgfile.version = get_FLOAT(&p);
    if (ROUND(10*imgfile.version) == 21)
        imgfile.header_size = 48;
    else
        imgfile.header_size = get_WORD(&p);
    imgfile.xres = get_WORD(&p);
    imgfile.yres = get_WORD(&p);
    imgfile.xrangemax = get_DWORD(&p);
    imgfile.yrangemax = get_DWORD(&p);
    imgfile.zrangemax = get_DWORD(&p);
    imgfile.xrange = get_DWORD(&p);
    imgfile.yrange = get_DWORD(&p);
    imgfile.zrange = get_DWORD(&p);
    imgfile.data_type = get_WORD(&p);
    imgfile.afm_head_id = get_WORD(&p);
    imgfile.zoom_factor = get_WORD(&p);
    imgfile.z_gain = get_WORD(&p);
    if (size != 2*imgfile.xres*imgfile.yres + imgfile.header_size) {
        err_SIZE_MISMATCH(error,
                          2*imgfile.xres*imgfile.yres + imgfile.header_size,
                          size);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = gwy_data_field_new(imgfile.xres, imgfile.yres,
                                Angstrom*imgfile.xrange,
                                Angstrom*imgfile.yrange,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    d = (gint16*)(buffer + imgfile.header_size);
    scale = imgfile.z_gain * imgfile.zrange;
    for (i = 0; i < imgfile.xres*imgfile.yres; i++)
        data[i] = scale * GINT16_FROM_LE(d[i]);

    gwy_file_abandon_contents(buffer, size, NULL);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_duplicate(unit);
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
