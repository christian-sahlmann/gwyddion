/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Date conversion code copied from Wine, see below.
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
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "DIGITAL SURF"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".sur"

enum { SURF_HEADER_SIZE = 512 };

typedef enum {
    SURF_PC        = 0,
    SURF_MACINTOSH = 257
} SurfFormatType;

typedef enum {
    SURF_PROFILE         = 1,
    SURF_SURFACE         = 2,
    SURF_BINARY          = 3,
    SURF_SERIES_PROFILES = 4,
    SURF_SERIES_SURFACES = 5
} SurfObjectType;

typedef enum {
    SURF_ACQ_UNKNOWN        = 0,
    SURF_ACQ_STYLUS         = 1,
    SURF_ACQ_OPTICAL        = 2,
    SURF_ACQ_THERMOCOUPLE   = 3,
    SURF_ACQ_UNKNOWN_TOO    = 4,
    SURF_ACQ_STYLUS_SKID    = 5,
    SURF_ACQ_AFM            = 6,
    SURF_ACQ_STM            = 7,
    SURF_ACQ_VIDEO          = 8,
    SURF_ACQ_INTERFEROMETER = 9,
    SURF_ACQ_LIGHT          = 10,
} SurfAcqusitionType;

typedef enum {
    SURF_RANGE_NORMAL = 0,
    SURF_RANGE_HIGH   = 1,
} SurfRangeType;

typedef enum {
    SURF_SP_NORMAL      = 0,
    SURF_SP_SATURATIONS = 1,
} SurfSpecialPointsType;

typedef enum {
    SURF_INV_NONE = 0,
    SURF_INV_Z    = 1,
    SURF_FLIP_Z   = 2,
    SURF_FLOP_Z   = 3,
} SurfInversionType;

typedef enum {
    SURF_LEVELING_NONE = 0,
    SURF_LEVELING_LSM  = 1,
    SURF_LEVELING_MZ   = 2,
} SurfLevelingType;


typedef struct {
    SurfFormatType format;
    gint nobjects;
    gint version;
    SurfObjectType type;
    gchar object_name[30];
    gchar operator_name[30];
    gint material_code;
    SurfAcqusitionType acquisition;
    SurfRangeType range;
    SurfSpecialPointsType special_points;
    gboolean absolute;
    gint pointsize;
    gint zmin;
    gint zmax;
    gint xres; /* number of points per line */
    gint yres; /* number of lines */
    gint nofpoints;
    gdouble dx;
    gdouble dy;
    gdouble dz;
    gchar xaxis[16];
    gchar yaxis[16];
    gchar zaxis[16];
    gchar dx_unit[16];
    gchar dy_unit[16];
    gchar dz_unit[16];
    gchar xlength_unit[16];
    gchar ylength_unit[16];
    gchar zlength_unit[16];
    gdouble xunit_ratio;
    gdouble yunit_ratio;
    gdouble zunit_ratio;
    gint imprint;
    SurfInversionType inversion;
    SurfLevelingType leveling;
    gint seconds;
    gint minutes;
    gint hours;
    gint day;
    gint month;
    gint year;
    gint measurement_duration;
    gint comment_size;
    gint private_size;
    gchar client_zone[128];
    gdouble XOffset;
    gdouble YOffset;
    gdouble ZOffset;
    GwyDataField *dfield;
} SurfFile;

static gboolean      module_register      (void);
static gint          surffile_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* surffile_load        (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      fill_data_fields     (SurfFile *surffile,
                                           const guchar *buffer,
                                           GError **error);
static GwyContainer* surffile_get_metadata(SurfFile *surffile);

static const GwyEnum acq_modes[] = {
   { "Unknown",                     SURF_ACQ_UNKNOWN,        },
   { "Contact stylus",              SURF_ACQ_STYLUS,         },
   { "Scanning optical gauge",      SURF_ACQ_OPTICAL,        },
   { "Thermocouple",                SURF_ACQ_THERMOCOUPLE,   },
   { "Unknown",                     SURF_ACQ_UNKNOWN_TOO,    },
   { "Contact stylus with skid",    SURF_ACQ_STYLUS_SKID,    },
   { "AFM",                         SURF_ACQ_AFM,            },
   { "STM",                         SURF_ACQ_STM,            },
   { "Video",                       SURF_ACQ_VIDEO,          },
   { "Interferometer",              SURF_ACQ_INTERFEROMETER, },
   { "Structured light projection", SURF_ACQ_LIGHT,          },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Surf data files."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("surffile",
                           N_("Surf files (.sur)"),
                           (GwyFileDetectFunc)&surffile_detect,
                           (GwyFileLoadFunc)&surffile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
surffile_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE)
        && fileinfo->file_size >= SURF_HEADER_SIZE + 2)
        score = 100;

    return score;
}

static GwyContainer*
surffile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    SurfFile surffile;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize expected_size, size = 0;
    GError *err = NULL;
    gchar signature[12];
    gdouble max, min;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }

    if (size < SURF_HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;

    get_CHARARRAY(signature, &p);
    if (strncmp(signature, "DIGITAL SURF", 12) != 0) {
        err_FILE_TYPE(error, "Surf");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    surffile.format = get_WORD_LE(&p);
    surffile.nobjects = get_WORD_LE(&p);
    surffile.version = get_WORD_LE(&p);
    surffile.type = get_WORD_LE(&p);
    get_CHARS0(surffile.object_name, &p, 30);
    get_CHARS0(surffile.operator_name, &p, 30);
    surffile.material_code = get_WORD_LE(&p);
    surffile.acquisition = get_WORD_LE(&p);
    surffile.range = get_WORD_LE(&p);
    surffile.special_points = get_WORD_LE(&p);
    surffile.absolute = get_WORD_LE(&p);
    /*reserved*/
    p += 8;
    surffile.pointsize = get_WORD_LE(&p);
    surffile.zmin = get_DWORD_LE(&p);
    surffile.zmax = get_DWORD_LE(&p);
    surffile.xres = get_DWORD_LE(&p);
    surffile.yres = get_DWORD_LE(&p);
    surffile.nofpoints = get_DWORD_LE(&p);

    surffile.dx = get_FLOAT_LE(&p);
    surffile.dy = get_FLOAT_LE(&p);
    surffile.dz = get_FLOAT_LE(&p);
    get_CHARS0(surffile.xaxis, &p, 16);
    get_CHARS0(surffile.yaxis, &p, 16);
    get_CHARS0(surffile.zaxis, &p, 16);
    get_CHARS0(surffile.dx_unit, &p, 16);
    get_CHARS0(surffile.dy_unit, &p, 16);
    get_CHARS0(surffile.dz_unit, &p, 16);
    get_CHARS0(surffile.xlength_unit, &p, 16);
    get_CHARS0(surffile.ylength_unit, &p, 16);
    get_CHARS0(surffile.zlength_unit, &p, 16);

    surffile.xunit_ratio = get_FLOAT_LE(&p);
    surffile.yunit_ratio = get_FLOAT_LE(&p);
    surffile.zunit_ratio = get_FLOAT_LE(&p);
    surffile.imprint = get_WORD_LE(&p);
    surffile.inversion = get_WORD_LE(&p);
    surffile.leveling = get_WORD_LE(&p);

    p += 12;

    surffile.seconds = get_WORD_LE(&p);
    surffile.minutes = get_WORD_LE(&p);
    surffile.hours = get_WORD_LE(&p);
    surffile.day = get_WORD_LE(&p);
    surffile.month = get_WORD_LE(&p);
    surffile.year = get_WORD_LE(&p);
    surffile.measurement_duration = get_WORD_LE(&p);
    surffile.comment_size = get_WORD_LE(&p);
    surffile.private_size = get_WORD_LE(&p);

    get_CHARARRAY(surffile.client_zone, &p);

    surffile.XOffset = get_FLOAT_LE(&p);
    surffile.YOffset = get_FLOAT_LE(&p);
    surffile.ZOffset = get_FLOAT_LE(&p);

    gwy_debug("fileformat: %d,  n_of_objects: %d, "
              "version: %d, object_type: %d",
              surffile.format, surffile.nobjects,
              surffile.version, surffile.type);
    gwy_debug("object name: <%s>", surffile.object_name);
    gwy_debug("operator name: <%s>", surffile.operator_name);

    gwy_debug("material code: %d, acquisition type: %d",
              surffile.material_code, surffile.acquisition);
    gwy_debug("range type: %d, special points: %d, absolute: %d",
              surffile.range,
              surffile.special_points, (gint)surffile.absolute);
    gwy_debug("data point size: %d", surffile.pointsize);
    gwy_debug("zmin: %d, zmax: %d", surffile.zmin, surffile.zmax);
    gwy_debug("xres: %d, yres: %d (xres*yres = %d)",
              surffile.xres, surffile.yres, (surffile.xres*surffile.yres));
    gwy_debug("total number of points: %d", surffile.nofpoints);
    gwy_debug("dx: %g, dy: %g, dz: %g",
              surffile.dx, surffile.dy, surffile.dz);
    gwy_debug("X axis name: %16s", surffile.xaxis);
    gwy_debug("Y axis name: %16s", surffile.yaxis);
    gwy_debug("Z axis name: %16s", surffile.zaxis);
    gwy_debug("dx unit: %16s", surffile.dx_unit);
    gwy_debug("dy unit: %16s", surffile.dy_unit);
    gwy_debug("dz unit: %16s", surffile.dz_unit);
    gwy_debug("X axis unit: %16s", surffile.xlength_unit);
    gwy_debug("Y axis unit: %16s", surffile.ylength_unit);
    gwy_debug("Z axis unit: %16s", surffile.zlength_unit);
    gwy_debug("xunit_ratio: %g, yunit_ratio: %g, zunit_ratio: %g",
              surffile.xunit_ratio, surffile.yunit_ratio, surffile.zunit_ratio);
    gwy_debug("imprint: %d, inversion: %d, leveling: %d",
              surffile.imprint, surffile.inversion, surffile.leveling);
    gwy_debug("Time: %d:%d:%d, Date: %d.%d.%d",
              surffile.hours, surffile.minutes, surffile.seconds,
              surffile.day, surffile.month, surffile.year);

    expected_size = (SURF_HEADER_SIZE
                     + surffile.pointsize/8*surffile.xres*surffile.yres);
    if (expected_size != size) {
        err_SIZE_MISMATCH(error, expected_size, size);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer + SURF_HEADER_SIZE;
    if (!fill_data_fields(&surffile, p, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!surffile.absolute) {
        max = gwy_data_field_get_max(surffile.dfield);
        min = gwy_data_field_get_min(surffile.dfield);
        gwy_data_field_add(surffile.dfield, -min);

        gwy_data_field_multiply(surffile.dfield,
                                (surffile.zmax - surffile.zmin)/(max-min));
    }

    switch (surffile.inversion) {
        case SURF_INV_Z:
        gwy_data_field_invert(surffile.dfield, FALSE, FALSE, TRUE);
        break;

        case SURF_FLIP_Z:
        gwy_data_field_invert(surffile.dfield, FALSE, TRUE, TRUE);
        break;

        case SURF_FLOP_Z:
        gwy_data_field_invert(surffile.dfield, TRUE, FALSE, TRUE);
        break;

        default:
        break;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", surffile.dfield);
    g_object_unref(surffile.dfield);

    meta = surffile_get_metadata(&surffile);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    gwy_app_channel_check_nonsquare(container, 0);

    return container;
}

static gboolean
fill_data_fields(SurfFile *surffile,
                 const guchar *buffer,
                 GError **error)
{
    GwySIUnit *siunit;
    gdouble *data;
    guint i, j;

    surffile->dfield = gwy_data_field_new(surffile->xres,
                                          surffile->yres,
                                          surffile->xres*surffile->dx,
                                          surffile->yres*surffile->dy,
                                          FALSE);

    data = gwy_data_field_get_data(surffile->dfield);
    switch (surffile->pointsize) {
        case 16:
        {
            const gint16 *row, *d16 = (const gint16*)buffer;

            for (i = 0; i < surffile->xres; i++) {
                row = d16 + i*surffile->yres;
                for (j = 0; j < surffile->yres; j++)
                    *(data++) = GINT16_FROM_LE(row[j]) * surffile->dz;
            }
        }
        break;

        case 32:
        {
            const gint32 *row, *d32 = (const gint32*)buffer;

            for (i = 0; i < surffile->xres; i++) {
                row = d32 + i*surffile->yres;
                for (j = 0; j < surffile->yres; j++)
                    *(data++) = GINT32_FROM_LE(row[j]) * surffile->dz;
            }
        }
        break;

        default:
        err_BPP(error, surffile->pointsize);
        return FALSE;
        break;
    }

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(surffile->dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(surffile->dfield, siunit);
    g_object_unref(siunit);

    return TRUE;
}

#define HASH_STORE(key, fmt, field) \
    gwy_container_set_string_by_name(meta, key, \
                                     g_strdup_printf(fmt, surffile->field))

static GwyContainer*
surffile_get_metadata(SurfFile *surffile)
{
    GwyContainer *meta;
    char date[40];

    meta = gwy_container_new();

    g_snprintf(date, sizeof(date), "%d. %d. %d",
               surffile->day, surffile->month, surffile->year);

    HASH_STORE("Version", "%u", version);
    HASH_STORE("Operator name", "%s", operator_name);
    HASH_STORE("Object name", "%s", object_name);
    gwy_container_set_string_by_name(meta, "Date", g_strdup(date));
    gwy_container_set_string_by_name
                (meta, "Acquisition type",
                  g_strdup(gwy_enum_to_string(surffile->acquisition, acq_modes,
                                              G_N_ELEMENTS(acq_modes))));

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
