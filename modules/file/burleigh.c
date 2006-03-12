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

#define HEADER_SIZE_V21 8
#define FOOTER_SIZE_V21 40
#define TOTAL_SIZE_V21 (HEADER_SIZE_V21 + FOOTER_SIZE_V21)

#define Angstrom (1e-10)

enum {
    BURLEIGH_CURRENT = 0,
    BURLEIGH_TOPOGRAPHY = 1
};

typedef struct {
    gdouble version;
    guint version_int;
    guint32 header_size;  /* Not in v2.1 */
    guint32 xres;
    guint32 yres;

    /* In v2.1, this in in the footer */
    guint32 xrangemax;
    guint32 yrangemax;
    guint32 zrangemax;
    guint32 xrange;
    guint32 yrange;
    guint32 zrange;
    guint32 data_type;
    guint32 scan_speed;
    gdouble z_gain;

    /* Not in v2.1 */
    guint32 afm_head_id;
    gdouble zoom_factor;

    /* Not in the older version described at weizmann.ac.il */
    guint32 zoom_level;
    guint32 bias_volts;
    guint32 tunneling_current;
} IMGFile;

static gboolean      module_register(void);
static gint          burleigh_detect(const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* burleigh_load  (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gint16* burleigh_load_v21(IMGFile *imgfile,
                                 const guchar *buffer,
                                 gsize size,
                                 GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh IMG data files version 2.1."),
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
                           N_("Burleigh 2.1 files (.img)"),
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
    guint version_int, xres, yres;
    gdouble version;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < 4)
        return 0;

    buffer = fileinfo->buffer;
    version = get_FLOAT(&buffer);
    version_int = ROUND(10*version);
    gwy_debug("Version: %g", version);

    if (version_int == 21) {
        if (fileinfo->file_size < TOTAL_SIZE_V21 + 2)
            return 0;

        xres = get_WORD(&buffer);
        yres = get_WORD(&buffer);
        if (fileinfo->file_size == TOTAL_SIZE_V21 + 2*xres*yres)
            return 95;
        return 0;
    }

    return 0;
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

    memset(&imgfile, 0, sizeof(imgfile));
    p = buffer;
    imgfile.version = get_FLOAT(&p);
    imgfile.version_int = ROUND(10*imgfile.version);
    if (imgfile.version_int == 21) {
        d = burleigh_load_v21(&imgfile, buffer, size, error);
        if (!d) {
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File format version %.1f is not supported."),
                    imgfile.version);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = gwy_data_field_new(imgfile.xres, imgfile.yres,
                                Angstrom*imgfile.xrange,
                                Angstrom*imgfile.yrange,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    scale = Angstrom * imgfile.z_gain * imgfile.zrange;
    for (i = 0; i < imgfile.xres*imgfile.yres; i++)
        data[i] = scale * GINT16_FROM_LE(d[i]);

    gwy_file_abandon_contents(buffer, size, NULL);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    container = gwy_container_new();
    switch (imgfile.data_type) {
        case BURLEIGH_CURRENT:
        unit = gwy_si_unit_new("A");
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Current"));
        break;

        case BURLEIGH_TOPOGRAPHY:
        unit = gwy_si_unit_new("m");
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Topography"));
        break;

        default:
        unit = gwy_si_unit_new("m");
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    return container;
}

static gint16*
burleigh_load_v21(IMGFile *imgfile,
                  const guchar *buffer,
                  gsize size,
                  GError **error)
{
    const guchar *p = buffer + 4; /* size of version */
    guint32 n;

    /* Header */
    imgfile->xres = get_WORD(&p);
    imgfile->yres = get_WORD(&p);
    n = imgfile->xres * imgfile->yres;
    if (size != 2*n + TOTAL_SIZE_V21) {
        err_SIZE_MISMATCH(error, 2*n + TOTAL_SIZE_V21, size);
        return NULL;
    }
    /* Skip to footer */
    p += 2*n;
    imgfile->xrangemax = get_DWORD(&p);
    imgfile->yrangemax = get_DWORD(&p);
    imgfile->zrangemax = get_DWORD(&p);
    imgfile->xrange = get_DWORD(&p);
    imgfile->yrange = get_DWORD(&p);
    imgfile->zrange = get_DWORD(&p);
    imgfile->scan_speed = get_WORD(&p);
    imgfile->zoom_level = get_WORD(&p);
    imgfile->data_type = get_WORD(&p);
    imgfile->z_gain = get_WORD(&p);
    imgfile->bias_volts = get_FLOAT(&p);
    imgfile->tunneling_current = get_FLOAT(&p);

    return (gint16*)(buffer + HEADER_SIZE_V21);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
