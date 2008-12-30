/*
 *  $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Roughly based on code in Kasgira by MV <kasigra@seznam.cz>.
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
 * <mime-type type="application/x-spmlab-spm">
 *   <comment>SPMLab SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="#R3"/>
 *     <match type="string" offset="0" value="#R4"/>
 *     <match type="string" offset="0" value="#R5"/>
 *     <match type="string" offset="0" value="#R6"/>
 *     <match type="string" offset="0" value="#R7"/>
 *   </magic>
 *   <glob pattern="*.zfp"/>
 *   <glob pattern="*.zrp"/>
 *   <glob pattern="*.zfr"/>
 *   <glob pattern="*.zrr"/>
 *   <glob pattern="*.ffp"/>
 *   <glob pattern="*.frp"/>
 *   <glob pattern="*.ffr"/>
 *   <glob pattern="*.frr"/>
 *   <glob pattern="*.lfp"/>
 *   <glob pattern="*.lrp"/>
 *   <glob pattern="*.lfr"/>
 *   <glob pattern="*.lrr"/>
 *   <glob pattern="*.sfp"/>
 *   <glob pattern="*.srp"/>
 *   <glob pattern="*.sfr"/>
 *   <glob pattern="*.srr"/>
 *   <glob pattern="*.1fp"/>
 *   <glob pattern="*.1rp"/>
 *   <glob pattern="*.1fr"/>
 *   <glob pattern="*.1rr"/>
 *   <glob pattern="*.2fp"/>
 *   <glob pattern="*.2rp"/>
 *   <glob pattern="*.2fr"/>
 *   <glob pattern="*.2rr"/>
 *   <glob pattern="*.ZFP"/>
 *   <glob pattern="*.ZRP"/>
 *   <glob pattern="*.ZFR"/>
 *   <glob pattern="*.ZRR"/>
 *   <glob pattern="*.FFP"/>
 *   <glob pattern="*.FRP"/>
 *   <glob pattern="*.FFR"/>
 *   <glob pattern="*.FRR"/>
 *   <glob pattern="*.LFP"/>
 *   <glob pattern="*.LRP"/>
 *   <glob pattern="*.LFR"/>
 *   <glob pattern="*.LRR"/>
 *   <glob pattern="*.SFP"/>
 *   <glob pattern="*.SRP"/>
 *   <glob pattern="*.SFR"/>
 *   <glob pattern="*.SRR"/>
 *   <glob pattern="*.1FP"/>
 *   <glob pattern="*.1RP"/>
 *   <glob pattern="*.1FR"/>
 *   <glob pattern="*.1RR"/>
 *   <glob pattern="*.2FP"/>
 *   <glob pattern="*.2RP"/>
 *   <glob pattern="*.2FR"/>
 *   <glob pattern="*.2RR"/>
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

static gboolean      module_register(void);
static gint          spmlab_detect  (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* spmlab_load    (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static GwyDataField* read_data_field(const guchar *buffer,
                                     guint size,
                                     guchar version,
                                     gchar **title,
                                     gint *direction,
                                     GError **error);
static gchar*        type_to_title  (guint type);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Thermicroscopes SpmLab R3 to R7 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.9",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("spmlab",
                           N_("Thermicroscopes SpmLab files"),
                           (GwyFileDetectFunc)&spmlab_detect,
                           (GwyFileLoadFunc)&spmlab_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
spmlab_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;

    if (only_name) {
        guint len;
        gchar ext[3];

        len = strlen(fileinfo->name_lowercase);
        if (len < 5)
            return 0;

        /* Match case insensitive *.[12zfls][fr][rp] */
        ext[0] = fileinfo->name_lowercase[len-3];
        ext[1] = fileinfo->name_lowercase[len-2];
        ext[2] = fileinfo->name_lowercase[len-1];
        if (fileinfo->name_lowercase[len-4] == '.'
            && (ext[2] == 'r' || ext[2] == 'p')
            && (ext[1] == 'f' || ext[1] == 'r')
            && (ext[0] == '1' || ext[0] == '2' || ext[0] == 'z'
                || ext[0] == 'f' || ext[0] == 'l' || ext[0] == 's'))
            score = 15;
        return score;
    }

    if (fileinfo->buffer_len >= 2048
        && fileinfo->head[0] == '#'
        && fileinfo->head[1] == 'R'
        && fileinfo->head[2] >= '3'
        && fileinfo->head[2] <= '7'
        && memchr(fileinfo->head+1, '#', 11))
        score = 15;   /* XXX: must be below plug-in score to allow overriding */

    return score;
}

static GwyContainer*
spmlab_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *title;
    gint dir;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    /* 2048 is wrong. moreover it differs for r5 and r4, kasigra uses 5752 for
     * r5 */
    if (size < 2048) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    if (buffer[0] != '#' || buffer[1] != 'R') {
        err_FILE_TYPE(error, "Thermicroscopes SpmLab");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    switch (buffer[2]) {
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        dfield = read_data_field(buffer, size, buffer[2], &title, &dir, error);
        break;

        default:
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown format version %c."), buffer[2]);
        break;
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    if (title)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(title));
    else
        gwy_app_channel_title_fall_back(container, 0);

    /* TODO: Store direction to metadata, if known */

    return container;
}

static gdouble
get_gfloat_le_as_double(const guchar **p)
{
    return gwy_get_gfloat_le(p);
}

static GwyDataField*
read_data_field(const guchar *buffer,
                guint size,
                guchar version,
                gchar **title,
                gint *direction,
                GError **error)
{
    enum { MIN_REMAINDER = 2620 };
    /* information offsets in different versions, in r5+ relative to data
     * start, in order:
     * data offset,
     * pixel dimensions,
     * physical dimensions,
     * value multiplier,
     * unit string,
     * data type,       (if zero, use channel title)
     * channel title    (if zero, use data type)
     */
    const guint offsets34[] = {
        0x0104, 0x0196, 0x01a2, 0x01b2, 0x01c2, 0x0400, 0x0000
    };
    const guint offsets56[] = {
        0x0104, 0x025c, 0x0268, 0x0288, 0x02a0, 0x0708, 0x0000
    };
    const guint offsets7[] = {
        0x0104, 0x029c, 0x02a8, 0x02c8, 0x02e0, 0x0000, 0x0b90
    };
    gint xres, yres, doffset, i, power10, type;
    gdouble xreal, yreal, q, z0;
    GwyDataField *dfield;
    GwySIUnit *unitxy, *unitz;
    gdouble *data;
    const guint *offset;
    const guchar *p, *r, *last;
    /* get floats in single precision from r4 but double from r5+ */
    gdouble (*getflt)(const guchar**);

    *title = NULL;
    *direction = -1;

    if (version == '5' || version == '6' || version == '7') {
        /* There are more headers in r5,
         * try to find something that looks like #R5. */
        last = r = buffer;
        while ((p = memchr(r, '#', size - (r - buffer) - MIN_REMAINDER))) {
            if (p[1] == 'R' && p[2] == version && p[3] == '.') {
                gwy_debug("pos: %ld", (long)(p - buffer));
                last = p;
                r = p + MIN_REMAINDER-1;
            }
            else
                r = p + 1;
        }
        offset = (version == '7') ? &offsets7[0] : &offsets56[0];
        buffer = last;
        getflt = &gwy_get_gdouble_le;
    }
    else {
        offset = &offsets34[0];
        getflt = &get_gfloat_le_as_double;
    }

    p = buffer + *(offset++);
    doffset = gwy_get_guint32_le(&p);  /* this appears to be the same number
                                          as in the ASCII miniheader -- so get
                                          it here since it's easier */
    gwy_debug("data offset = %u", doffset);
    p = buffer + *(offset++);
    xres = gwy_get_guint32_le(&p);
    yres = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;
    p = buffer + *(offset++);
    xreal = -getflt(&p);
    xreal += getflt(&p);
    yreal = -getflt(&p);
    yreal += getflt(&p);
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }
    p = buffer + *(offset++);
    q = getflt(&p);
    z0 = getflt(&p);
    gwy_debug("xreal.raw = %g, yreal.raw = %g, q.raw = %g, z0.raw = %g",
              xreal, yreal, q, z0);
    p = buffer + *(offset++);
    unitz = gwy_si_unit_new_parse(p, &power10);
    q *= pow10(power10);
    z0 *= pow10(power10);
    unitxy = gwy_si_unit_new_parse(p + 10, &power10);
    xreal *= pow10(power10);
    yreal *= pow10(power10);
    gwy_debug("xres = %d, yres = %d, xreal = %g, yreal = %g, q = %g, z0 = %g",
              xres, yres, xreal, yreal, q, z0);
    gwy_debug("unitxy = %s, unitz = %s", p, p + 10);

    if (offset[1]) {
        /* We know channel title */
        offset++;
        p = buffer + *(offset++);
        *title = g_strndup(p, size - (p - buffer));
        gwy_debug("title = <%s>", *title);
    }
    else {
        /* We know data type */
        p = buffer + *(offset++);
        type = gwy_get_guint16_le(&p);
        *direction = gwy_get_guint16_le(&p);
        gwy_debug("type = %d, dir = %d", type, *direction);
        offset++;
        *title = type_to_title(type);
    }

    p = buffer + doffset;
    if (err_SIZE_MISMATCH(error, 2*xres*yres, size - (p - buffer), FALSE))
        return NULL;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    g_object_unref(unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++)
        data[i] = (p[2*i] + 256.0*p[2*i + 1])*q + z0;

    return dfield;
}

static gchar*
type_to_title(guint type)
{
    const gchar *title;

    title = gwy_enuml_to_string(type,
                                "Height", 0,
                                "Current", 1,
                                "FFM", 2,
                                "Spect", 3,
                                "SpectV", 4,
                                "ADC1", 5,
                                "ADC2", 6,
                                "TipV", 7,
                                "DAC1", 8,
                                "DAC2", 9,
                                "ZPiezo", 10,
                                "Height error", 11,
                                "Linearized Z", 12,
                                "Feedback", 13,
                                NULL);
    if (*title)
        return g_strdup(title);

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

