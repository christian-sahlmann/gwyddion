/*
 *  $Id$
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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".img"

#define MAGIC "RSCOPE"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define Angstrom (1e-10)

enum {
    HEADER_SIZE = 2048
};

typedef struct {
    gchar program_name[6];
    guint32 file_version;
    guint32 program_version;
    guint32 header_size;
    guint32 data_size;
    gchar time[17];
    gchar comment[149];
    guint data_type;  /* FIXME */
    guint32 xres;
    guint32 yres;
    gdouble xreal;
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gchar title[19];
    gdouble sample_pause;
    gdouble sample_speed;
    gdouble tunnel_current;
    gdouble bias;
    gdouble loop_gain;
    guint direction;  /* FIXME */
    guint head_type;  /* FIXME */
    gdouble x_calibration;
    gdouble z_calibration;
    gdouble min;
    gdouble max;
    gdouble mean;
    gdouble full_scale;
    gdouble scale_offset;
    gdouble x_slope_corr;
    gdouble y_slope_corr;
    gdouble offset_corr;
    gboolean slope_calculated;
    gboolean roughness_valid;
    gdouble ra;
    gdouble rms;
    gdouble ry;
    guint display_form_mode;  /* FIXME */
    guint display_rotated;  /* FIXME */
    guint32 display_angle_polar;
    guint32 display_angle_azimuthal;
    gdouble scale_fraction;
    guint slope_mode;  /* FIXME */
    gdouble height_scale_factor;
} DMEFile;

static gboolean      module_register(void);
static gint          dme_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* dme_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static void          dme_read_header(const guchar *p,
                                     DMEFile *dmefile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Pacific Nanotechnology DME data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("dmefile",
                           N_("DME files (.dme)"),
                           (GwyFileDetectFunc)&dme_detect,
                           (GwyFileLoadFunc)&dme_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
dme_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size > HEADER_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
dme_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const guchar *p;
    gint i, xres, yres;
    DMEFile dmefile;
    gdouble xreal, yreal, zscale;
    gdouble *data;
    const gint16 *d16;
    GwySIUnit *siunit;
    const gchar *title;
    gchar *s;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dme_read_header(buffer, &dmefile);

    gwy_file_abandon_contents(buffer, size, NULL);
    err_NO_DATA(error);
    return NULL;

#if 0
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    d16 = (const gint16*)(buffer + DATA_START);
    for (i = 0; i < xres*yres; i++)
        data[i] = zscale*d16[i]/65536.0;
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    gwy_file_abandon_contents(buffer, size, NULL);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    switch (value_type) {
        case VALUE_TYPE_NM:
        siunit = gwy_si_unit_new("m");
        gwy_data_field_multiply(dfield, Nanometer);
        break;

        case VALUE_TYPE_MV:
        siunit = gwy_si_unit_new("V");
        gwy_data_field_multiply(dfield, Milivolt);
        break;

        default:
        g_warning("Value type %d is unknown", value_type);
        siunit = gwy_si_unit_new(NULL);
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    title = gwy_enum_to_string(data_type, titles, G_N_ELEMENTS(titles));
    if (title) {
        s = g_strdup_printf("%s (%s)",
                            title,
                            direction ? "Forward" : "Backward");
        gwy_container_set_string_by_name(container, "/0/data/title", s);
    }
    else
        g_warning("Data type %d is unknown", data_type);

    return container;
#endif
}

static void
dme_read_header(const guchar *p,
                DMEFile *dmefile)
{
    const guchar *q = p;

    get_CHARARRAY(dmefile->program_name, &p);
    dmefile->file_version = get_WORD_LE(&p);
    dmefile->program_version = get_WORD_LE(&p);
    dmefile->header_size = get_WORD_LE(&p);
    dmefile->data_size = get_DWORD_LE(&p);
    gwy_debug("header_size: %u data_size: %u",
              dmefile->header_size, dmefile->data_size);
    get_CHARARRAY(dmefile->time, &p);
    get_CHARARRAY0(dmefile->comment, &p);
    dmefile->data_type = *(p++);
    p += 123;  /* reserved */
    dmefile->xres = get_DWORD_LE(&p);
    dmefile->yres = get_DWORD_LE(&p);
    gwy_debug("xres: %u, yres: %u", dmefile->xres, dmefile->yres);
    dmefile->xreal = get_PASCAL_REAL_LE(&p);
    dmefile->yreal = get_PASCAL_REAL_LE(&p);
    gwy_debug("xreal: %g, yreal: %g", dmefile->xreal, dmefile->yreal);
    dmefile->xoff = get_PASCAL_REAL_LE(&p);
    dmefile->yoff = get_PASCAL_REAL_LE(&p);
    get_CHARARRAY0(dmefile->title, &p);
    dmefile->sample_pause = get_PASCAL_REAL_LE(&p);
    dmefile->sample_speed = get_PASCAL_REAL_LE(&p);
    dmefile->tunnel_current = get_PASCAL_REAL_LE(&p);
    dmefile->bias = get_PASCAL_REAL_LE(&p);
    dmefile->loop_gain = get_PASCAL_REAL_LE(&p);
    dmefile->direction = get_WORD_LE(&p);
    dmefile->head_type = *(p++);
    p += 291;  /* reserved */
    dmefile->x_calibration = get_PASCAL_REAL_LE(&p);
    dmefile->z_calibration = get_PASCAL_REAL_LE(&p);
    gwy_debug("x_calibration: %g, z_calibration: %g",
              dmefile->x_calibration, dmefile->z_calibration);
    p += 120;  /* reserved */
    dmefile->min = get_PASCAL_REAL_LE(&p);
    dmefile->max = get_PASCAL_REAL_LE(&p);
    dmefile->mean = get_PASCAL_REAL_LE(&p);
    dmefile->full_scale = get_PASCAL_REAL_LE(&p);
    dmefile->scale_offset = get_PASCAL_REAL_LE(&p);
    dmefile->x_slope_corr = get_PASCAL_REAL_LE(&p);
    dmefile->y_slope_corr = get_PASCAL_REAL_LE(&p);
    dmefile->offset_corr = get_PASCAL_REAL_LE(&p);
    dmefile->slope_calculated = get_BBOOLEAN(&p);
    dmefile->roughness_valid = get_BBOOLEAN(&p);
    dmefile->ra = get_PASCAL_REAL_LE(&p);
    dmefile->rms = get_PASCAL_REAL_LE(&p);
    dmefile->ry = get_PASCAL_REAL_LE(&p);
    p += 2017;  /* reserved */
    dmefile->display_form_mode = *(p++);
    dmefile->display_rotated = get_WORD_LE(&p);
    dmefile->display_angle_polar = get_WORD_LE(&p);
    dmefile->display_angle_azimuthal = get_WORD_LE(&p);
    dmefile->scale_fraction = get_PASCAL_REAL_LE(&p);
    p += 38;  /* reserved */
    dmefile->slope_mode = *(p++);
    p += 38;  /* reserved */
    dmefile->height_scale_factor = get_PASCAL_REAL_LE(&p);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

