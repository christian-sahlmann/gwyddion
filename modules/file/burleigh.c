/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Hans-Peter Doerr.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *          doerr@cip.physik.uni-freiburg.de.
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

/*
 * See:
 * http://www.physik.uni-freiburg.de/~doerr/readimg
 * for description and the scaling confusion discussion.
 *
 * See
 * http://www.weizmann.ac.il/Chemical_Research_Support/surflab/peter/headers/burl.html
 * for description of an unspecified version which is NOT implemented.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-burleigh-spm">
 *   <comment>Burleigh SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\xff\x06@\x00"/>
 *   </magic>
 *   <glob pattern="*.img"/>
 *   <glob pattern="*.IMG"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".img"

#define HEADER_SIZE_MIN 42

#define HEADER_SIZE_V21 8
#define FOOTER_SIZE_V21 40
#define TOTAL_SIZE_V21 (HEADER_SIZE_V21 + FOOTER_SIZE_V21)

#define Angstrom (1e-10)
#define Picoampere (1e-12)

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

static gboolean      module_register  (void);
static gint          burleigh_detect  (const GwyFileDetectInfo *fileinfo,
                                       gboolean only_name);
static GwyContainer* burleigh_load    (const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);
static const gint16* burleigh_load_v21(IMGFile *imgfile,
                                       const guchar *buffer,
                                       gsize size,
                                       GError **error);
static gdouble burleigh_get_zoom_v21  (IMGFile *imgfile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh IMG data files version 2.1."),
    "Yeti <yeti@gwyddion.net>",
    "0.5",
    "David Nečas (Yeti) & Petr Klapetek & Hans-Peter Doerr",
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

    buffer = fileinfo->head;
    version = gwy_get_gfloat_le(&buffer);
    version_int = GWY_ROUND(10*version);
    gwy_debug("Version: %g", version);

    if (version_int == 21) {
        if (fileinfo->file_size < TOTAL_SIZE_V21 + 2)
            return 0;

        xres = gwy_get_guint16_le(&buffer);
        yres = gwy_get_guint16_le(&buffer);
        if (fileinfo->file_size == TOTAL_SIZE_V21 + 2*xres*yres)
            return 100;
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
    const gint16 *d;
    gdouble zoom;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE_MIN + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    memset(&imgfile, 0, sizeof(imgfile));
    p = buffer;
    imgfile.version = gwy_get_gfloat_le(&p);
    imgfile.version_int = GWY_ROUND(10*imgfile.version);
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

    zoom = burleigh_get_zoom_v21(&imgfile);
    if (err_DIMENSION(error, imgfile.xres)
        || err_DIMENSION(error, imgfile.yres)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = gwy_data_field_new(imgfile.xres, imgfile.yres,
                                Angstrom*imgfile.xrange/zoom,
                                Angstrom*imgfile.yrange/zoom,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < imgfile.xres*imgfile.yres; i++)
        data[i] = GINT16_FROM_LE(d[i])*imgfile.zrange/4095.0;

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
        gwy_data_field_multiply(dfield, Picoampere);
        break;

        case BURLEIGH_TOPOGRAPHY:
        unit = gwy_si_unit_new("m");
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Topography"));
        gwy_data_field_multiply(dfield, Angstrom);
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

static const gint16*
burleigh_load_v21(IMGFile *imgfile,
                  const guchar *buffer,
                  gsize size,
                  GError **error)
{
    const guchar *p = buffer + 4; /* size of version */
    guint32 n;

    /* Header */
    imgfile->xres = gwy_get_guint16_le(&p);
    imgfile->yres = gwy_get_guint16_le(&p);
    n = imgfile->xres * imgfile->yres;
    if (err_SIZE_MISMATCH(error, 2*n + TOTAL_SIZE_V21, size, TRUE))
        return NULL;

    /* Skip to footer */
    p += 2*n;
    imgfile->xrangemax = gwy_get_guint32_le(&p);
    imgfile->yrangemax = gwy_get_guint32_le(&p);
    imgfile->zrangemax = gwy_get_guint32_le(&p);
    imgfile->xrange = gwy_get_guint32_le(&p);
    imgfile->yrange = gwy_get_guint32_le(&p);
    gwy_debug("xrange: %u, yrange: %u", imgfile->xrange, imgfile->yrange);
    imgfile->zrange = gwy_get_guint32_le(&p);
    gwy_debug("zrange: %u", imgfile->zrange);
    imgfile->scan_speed = gwy_get_guint16_le(&p);
    imgfile->zoom_level = gwy_get_guint16_le(&p);
    gwy_debug("zoom_level: %u", imgfile->zoom_level);
    imgfile->data_type = gwy_get_guint16_le(&p);
    gwy_debug("data_type: %u", imgfile->data_type);
    imgfile->z_gain = gwy_get_guint16_le(&p);
    imgfile->bias_volts = gwy_get_gfloat_le(&p);
    imgfile->tunneling_current = gwy_get_gfloat_le(&p);

    /* Use negated positive conditions to catch NaNs */
    if (!((imgfile->xrange = fabs(imgfile->xrange)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        imgfile->xrange = 1.0;
    }
    if (!((imgfile->yrange = fabs(imgfile->yrange)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        imgfile->yrange = 1.0;
    }

    return (const gint16*)(buffer + HEADER_SIZE_V21);
}

static gdouble
burleigh_get_zoom_v21(IMGFile *imgfile)
{
    static const gdouble zooms[] = { 1.0, 2.0, 10.0, 50.0, 250.0 };

    if (imgfile->zoom_level >= 1
        && imgfile->zoom_level <= G_N_ELEMENTS(zooms))
        return zooms[imgfile->zoom_level-1];

    g_warning("Unknown zoom level %d, assuming zoom factor 1.0",
              imgfile->zoom_level);
    return 1.0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
