/*
 *  $Id$
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-surf-spm">
 *   <comment>Surf SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="DIGITAL SURF"/>
 *   </magic>
 *   <glob pattern="*.sur"/>
 *   <glob pattern="*.SUR"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/gwyapp.h>
#include <glib/gstdio.h>
#include <stdlib.h>


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
    guint nofpoints;
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
    gint dayof;
    gfloat measurement_duration;
    gint comment_size;
    gint private_size;
    gchar client_zone[128];
    gdouble XOffset;
    gdouble YOffset;
    gdouble ZOffset;
    GwyDataField *dfield;
    GwySIUnit *xyunit;
    GwySIUnit *zunit;
} SurfFile;


/*new structure surfwriter to write directly the structure with cast gint16 of enums */
typedef struct{
    char signature[12];
    gint16 format;
    guint16 nobjects;
    gint16 version;
    gint16 type;
    char object_name[30];
    char operator_name[30];
    gint16 material_code;
    gint16 acquisition;
    gint16 range;
    gint16 special_points;
    gint16 absolute;
    char reserved[8];
    gint16 pointsize;
    gint32 zmin;
    gint32 zmax;
    gint32 xres;
    gint32 yres;
    gint32 nofpoints;
    gfloat dx;
    gfloat dy;
    gfloat dz;
    char xaxis[16];
    char yaxis[16];
    char zaxis[16];
    char dx_unit[16];
    char dy_unit[16];
    char dz_unit[16];
    char xlength_unit[16];
    char ylength_unit[16];
    char zlength_unit[16];
    gfloat xunit_ratio;
    gfloat yunit_ratio;
    gfloat zunit_ratio;
    gint16 imprint;
    gint16 inversion;
    gint16 leveling;
    char obsolete[12];
    gint16 seconds;
    gint16 minutes;
    gint16 hours;
    gint16 day;
    gint16 month;
    gint16 year;
    gint16 dayof;
    gfloat measurement_duration;
    char obsolete2[10];
    gint16 comment_size;
    gint16 private_size;
    char client_zone[128];
    gfloat XOffset;
    gfloat YOffset;
    gfloat ZOffset;
    char reservedzone[34];
}SurfWriter;



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

static gboolean      surffile_save        (GwyContainer *data,
                                           const gchar *filename,
                                           G_GNUC_UNUSED GwyRunType mode,
                                           GError **error);
static void          get_unit             (char *unit,
                                           int *power,
                                           double ref );



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

//from gstreamer, floatcast.h
inline static gfloat GFLOAT_TO_LE(gfloat in) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    return in;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
//from gstreamer, floatcast.h
    gint32 swap;
    gfloat out;
    memcpy(&swap, &in, 4);
    swap = GUINT32_SWAP_LE_BE_CONSTANT (swap);
    memcpy(&out, &swap, 4);
    return out;
#else /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */
#error unknown ENDIAN type
#endif /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */
};



static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Surf data files."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "0.9",
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
                           (GwyFileSaveFunc)&surffile_save);

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
    gint coef = 0;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize expected_size, size = 0;
    GError *err = NULL;
    gchar signature[12];

    gint add = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
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

    surffile.format = gwy_get_guint16_le(&p);
    surffile.nobjects = gwy_get_guint16_le(&p);
    surffile.version = gwy_get_guint16_le(&p);
    surffile.type = gwy_get_guint16_le(&p);
    get_CHARS0(surffile.object_name, &p, 30);
    get_CHARS0(surffile.operator_name, &p, 30);
    surffile.material_code = gwy_get_guint16_le(&p);
    surffile.acquisition = gwy_get_guint16_le(&p);
    surffile.range = gwy_get_guint16_le(&p);
    surffile.special_points = gwy_get_guint16_le(&p);
    surffile.absolute = gwy_get_guint16_le(&p);
    /*reserved*/
    p += 8;
    surffile.pointsize = gwy_get_guint16_le(&p);
    surffile.zmin = gwy_get_gint32_le(&p);
    surffile.zmax = gwy_get_gint32_le(&p);
    surffile.xres = gwy_get_gint32_le(&p);
    surffile.yres = gwy_get_gint32_le(&p);
    surffile.nofpoints = gwy_get_guint32_le(&p);

    surffile.dx = gwy_get_gfloat_le(&p);
    surffile.dy = gwy_get_gfloat_le(&p);
    surffile.dz = gwy_get_gfloat_le(&p);
    get_CHARS0(surffile.xaxis, &p, 16);
    get_CHARS0(surffile.yaxis, &p, 16);
    get_CHARS0(surffile.zaxis, &p, 16);
    get_CHARS0(surffile.dx_unit, &p, 16);
    get_CHARS0(surffile.dy_unit, &p, 16);
    get_CHARS0(surffile.dz_unit, &p, 16);
    get_CHARS0(surffile.xlength_unit, &p, 16);
    get_CHARS0(surffile.ylength_unit, &p, 16);
    get_CHARS0(surffile.zlength_unit, &p, 16);

    surffile.xunit_ratio = gwy_get_gfloat_le(&p);
    surffile.yunit_ratio = gwy_get_gfloat_le(&p);
    surffile.zunit_ratio = gwy_get_gfloat_le(&p);
    surffile.imprint = gwy_get_guint16_le(&p);
    surffile.inversion = gwy_get_guint16_le(&p);
    surffile.leveling = gwy_get_guint16_le(&p);

    p += 12;

    surffile.seconds = gwy_get_guint16_le(&p);
    surffile.minutes = gwy_get_guint16_le(&p);
    surffile.hours = gwy_get_guint16_le(&p);
    surffile.day = gwy_get_guint16_le(&p);
    surffile.month = gwy_get_guint16_le(&p);
    surffile.year = gwy_get_guint16_le(&p);
    surffile.dayof = gwy_get_guint16_le(&p);
    surffile.measurement_duration = gwy_get_gfloat_le(&p);
    p += 10;

    surffile.comment_size = gwy_get_guint16_le(&p);
    surffile.private_size = gwy_get_guint16_le(&p);

    get_CHARARRAY(surffile.client_zone, &p);
    surffile.XOffset = gwy_get_gfloat_le(&p);
    surffile.YOffset = gwy_get_gfloat_le(&p);
    surffile.ZOffset = gwy_get_gfloat_le(&p);


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
    gwy_debug("private zone size: %d, comment size %d",
              surffile.private_size, surffile.comment_size);

    if (err_DIMENSION(error, surffile.xres)
        || err_DIMENSION(error, surffile.yres)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    expected_size = (SURF_HEADER_SIZE
                     + surffile.pointsize/8*surffile.xres*surffile.yres);
    if (expected_size != size) {
        gwy_debug("Size mismatch!");
        if (size > expected_size)
            add = size - expected_size; /*TODO  correct this !*/
        else {
            err_SIZE_MISMATCH(error, expected_size, size, TRUE);
            gwy_file_abandon_contents(buffer, size, NULL);
          return NULL;
        }
    }

    /* Use negated positive conditions to catch NaNs */
    if (!((surffile.dx = fabs(surffile.dx)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        surffile.dx = 1.0;
    }
    if (!((surffile.dy = fabs(surffile.dy)) > 0)) {
        g_warning("Real y step is 0.0, fixing to 1.0");
        surffile.dy = 1.0;
    }

    p = buffer + SURF_HEADER_SIZE + add;

    /*units*/
    coef = 0;
    surffile.xyunit = gwy_si_unit_new_parse(surffile.dx_unit, &coef);
    surffile.dx *= pow10(coef);
    coef = 0;
    surffile.xyunit = gwy_si_unit_new_parse(surffile.dy_unit, &coef);
    surffile.dy *= pow10(coef);

    coef = 0;
    surffile.zunit = gwy_si_unit_new_parse (surffile.dz_unit, &coef);
    if (!fill_data_fields(&surffile, p, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
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

    gwy_data_field_multiply(surffile.dfield, pow10(coef));
    surffile.ZOffset *=  pow10(coef);
    gwy_data_field_add(surffile.dfield, surffile.ZOffset);

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

    gwy_data_field_set_si_unit_xy(surffile->dfield, surffile->xyunit);
    g_object_unref(surffile->xyunit);

    gwy_data_field_set_si_unit_z(surffile->dfield, surffile->zunit);
    g_object_unref(surffile->zunit);

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

static gboolean
surffile_save(GwyContainer *data,
             const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    FILE *fh;
    gboolean ok = TRUE;
    SurfWriter surf;
    gfloat uintmax = 1073741824.0;
    gdouble zmaxreal;
    gdouble zminreal;
    gdouble xreal;
    gdouble yreal;
    gchar *dxunittmp;
    gchar *dyunittmp;
    gchar *dzunittmp;
    GwySIUnit *xysi;
    GwySIUnit *zsi;
    GwyContainer *current_data;
    int power = 0;
    int k = 0;
    GwyDataField *dfield;
    const gdouble *points;
    gint32 *integer_values;

    strncpy(surf.signature, "DIGITAL SURF", 12);
    surf.format = 0;
    surf.nobjects = 1;
    surf.version = 1;
    surf.type = 1;
    strncpy(surf.object_name, "SCRATCH", 30);
    strncpy(surf.operator_name, "csm", 30);
    surf.material_code = 0;
    surf.acquisition = 0;
    surf.range = 1;
    surf.special_points = 0;
    surf.absolute = 1;
    strncpy(surf.reserved, " ", 8);
    surf.pointsize = 32;
    surf.zmin = 0;
    surf.zmax = 1073741824.0;
    strncpy(surf.xaxis, "X", 16);
    strncpy(surf.yaxis, "Y", 16);
    strncpy(surf.zaxis, "Z", 16);
    surf.xunit_ratio = 1;
    surf.yunit_ratio = 1;
    surf.zunit_ratio = 1;
    surf.imprint = 1;
    surf.inversion = 0;
    surf.leveling = 0;
    strncpy(surf.obsolete, " ", 12);
    surf.seconds = 0;
    surf.minutes = 0;
    surf.hours = 0;
    surf.day = 5;
    surf.month = 1;
    surf.year = 2001;
    surf.dayof = 0;
    surf.measurement_duration = 1.0;
    strncpy(surf.obsolete2, " ", 10);
    surf.comment_size = 0;
    surf.private_size = 0;
    strncpy(surf.client_zone," ", 128);

    surf.XOffset = 0.0;
    surf.YOffset = 0.0;
    surf.ZOffset = 0.0;
    strncpy(surf.reservedzone, " ", 34);

    if (!(fh = g_fopen( filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &current_data, 0);
    if (!current_data) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    if (data != current_data) {
        return FALSE;
    }

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield, 0);
    if (!dfield) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    /*header values*/
    xysi = gwy_data_field_get_si_unit_xy(dfield);
    zsi = gwy_data_field_get_si_unit_z(dfield);
    dxunittmp = gwy_si_unit_get_string(xysi, GWY_SI_UNIT_FORMAT_PLAIN);
    dyunittmp = gwy_si_unit_get_string (xysi, GWY_SI_UNIT_FORMAT_PLAIN);
    dzunittmp = gwy_si_unit_get_string (zsi, GWY_SI_UNIT_FORMAT_PLAIN);
    strncpy(surf.dx_unit, dxunittmp, 16);
    strncpy(surf.dy_unit, dyunittmp, 16);
    strncpy(surf.dz_unit, dzunittmp, 16);
    g_free(dxunittmp);
    g_free(dyunittmp);
    g_free(dzunittmp);

    /*extrema*/
    zmaxreal = gwy_data_field_get_max(dfield);
    zminreal = gwy_data_field_get_min(dfield);
    surf.xres = gwy_data_field_get_xres(dfield);
    surf.yres = gwy_data_field_get_yres(dfield);
    surf.nofpoints = surf.xres * surf.yres;
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    /*units*/
    power = 0;
    surf.dx = (gfloat)(xreal / surf.xres);
    get_unit(surf.dx_unit, &power, xreal);
    surf.dx *= pow10(power);
    strncpy(surf.xlength_unit, surf.dx_unit, 16);
    power = 0;
    surf.dy = (gfloat)(yreal / surf.yres);
    get_unit(surf.dy_unit, &power, yreal);
    surf.dy *= pow10(power);
    strncpy(surf.ylength_unit, surf.dy_unit, 16);

    power = 0;
    if (zmaxreal > zminreal) {
        get_unit(surf.dz_unit, &power, (zmaxreal - zminreal));
    }
    strncpy(surf.zlength_unit, surf.dz_unit, 16);

    zmaxreal *= pow10(power);
    zminreal *= pow10(power);
    surf.dz = (zmaxreal - zminreal) / uintmax;
    surf.ZOffset = zminreal;

    /*convert data into integer32*/
    integer_values = g_new(gint32, surf.nofpoints);
    points = gwy_data_field_get_data_const(dfield);

    if (zminreal != zmaxreal) {
        for (k = 0; k < surf.nofpoints; k++) {  // * pow10( power ) to convert in the dz_unit
            integer_values[k] = floor(uintmax * (points[k] * pow10(power)  - zminreal) / (zmaxreal - zminreal));
        }
    }
    else {
        for (k = 0; k < surf.nofpoints; k++) {
            integer_values[k] = 0;
        }
    }


    /* byte order*/
    surf.format = GINT16_TO_LE(surf.format);
    surf.nobjects = GUINT16_TO_LE(surf.nobjects);
    surf.version = GINT16_TO_LE(surf.version);
    surf.type = GINT16_TO_LE(surf.type);
    surf.material_code = GINT16_TO_LE(surf.material_code);
    surf.acquisition = GINT16_TO_LE(surf.acquisition);
    surf.range = GINT16_TO_LE(surf.range);
    surf.special_points = GINT16_TO_LE(surf.special_points);
    surf.absolute = GINT16_TO_LE(surf.absolute);
    surf.pointsize = GINT16_TO_LE(surf.pointsize);
    surf.zmin = GINT32_TO_LE(surf.zmin);
    surf.zmax = GINT32_TO_LE(surf.zmax);
    surf.xres = GINT32_TO_LE(surf.xres);
    surf.yres = GINT32_TO_LE(surf.yres);
    surf.nofpoints = GINT32_TO_LE(surf.nofpoints);
    surf.dx = GFLOAT_TO_LE(surf.dx);
    surf.dy = GFLOAT_TO_LE(surf.dy);
    surf.dz = GFLOAT_TO_LE(surf.dz);
    surf.xunit_ratio = GFLOAT_TO_LE(surf.xunit_ratio);
    surf.yunit_ratio = GFLOAT_TO_LE(surf.yunit_ratio);
    surf.zunit_ratio = GFLOAT_TO_LE(surf.zunit_ratio);
    surf.imprint = GINT16_TO_LE(surf.imprint);
    surf.inversion = GINT16_TO_LE(surf.inversion);
    surf.leveling = GINT16_TO_LE(surf.leveling);
    surf.seconds = GINT16_TO_LE(surf.seconds);
    surf.minutes = GINT16_TO_LE(surf.minutes);
    surf.hours = GINT16_TO_LE(surf.hours);
    surf.day = GINT16_TO_LE(surf.day);
    surf.month = GINT16_TO_LE(surf.month);
    surf.year = GINT16_TO_LE(surf.year);
    surf.dayof = GINT16_TO_LE(surf.dayof);
    surf.measurement_duration = GFLOAT_TO_LE(surf.measurement_duration);
    surf.comment_size = GINT16_TO_LE(surf.comment_size);
    surf.private_size = GINT16_TO_LE(surf.private_size);
    surf.XOffset = GFLOAT_TO_LE(surf.XOffset);
    surf.YOffset = GFLOAT_TO_LE(surf.YOffset);
    surf.ZOffset = GFLOAT_TO_LE(surf.ZOffset);
    for (k = 0; k < surf.nofpoints; k++) {
        integer_values[k] = GINT32_TO_LE(integer_values[k]);
    }


//write

// fwrite(&surf, sizeof( SurfWriter ), 1, fh) bad struct align
    if(
        fwrite(&surf.signature, sizeof(char), 12, fh) != 12 ||
        fwrite(&surf.format, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.nobjects, sizeof(guint16), 1, fh) != 1 ||
        fwrite(&surf.version, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.type, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.object_name, sizeof(char), 30, fh) != 30 ||
        fwrite(&surf.operator_name, sizeof(char), 30, fh) != 30 ||
        fwrite(&surf.material_code, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.acquisition, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.range, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.special_points, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.absolute, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.reserved, sizeof(char), 8, fh) != 8 ||
        fwrite(&surf.pointsize, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.zmin, sizeof(gint32), 1, fh) != 1 ||
        fwrite(&surf.zmax, sizeof(gint32), 1, fh) != 1 ||
        fwrite(&surf.xres, sizeof(gint32), 1, fh) != 1 ||
        fwrite(&surf.yres, sizeof(gint32), 1, fh) != 1 ||
        fwrite(&surf.nofpoints, sizeof(gint32), 1, fh) != 1 ||
        fwrite(&surf.dx, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.dy, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.dz, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.xaxis, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.yaxis, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.zaxis, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.dx_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.dy_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.dz_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.xlength_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.ylength_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.zlength_unit, sizeof(char), 16, fh) != 16 ||
        fwrite(&surf.xunit_ratio, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.yunit_ratio, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.zunit_ratio, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.imprint, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.inversion, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.leveling, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.obsolete, sizeof(char), 12, fh) != 12 ||
        fwrite(&surf.seconds, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.minutes, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.hours, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.day, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.month, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.year, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.dayof, sizeof(gint16),1,fh) != 1 ||
        fwrite(&surf.measurement_duration, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.obsolete2, sizeof(char), 10, fh) != 10 ||
        fwrite(&surf.comment_size, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.private_size, sizeof(gint16), 1, fh) != 1 ||
        fwrite(&surf.client_zone, sizeof(char), 128, fh) != 128 ||
        fwrite(&surf.XOffset, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.YOffset, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.ZOffset, sizeof(gfloat), 1, fh) != 1 ||
        fwrite(&surf.reservedzone, sizeof(char), 34, fh)!= 34 ||
        fwrite(integer_values, sizeof(gint32), surf.nofpoints, fh) != surf.nofpoints
	    ) {
            err_WRITE(error);
            ok = FALSE;
            g_unlink(filename);
        }

    g_free(integer_values);
    fclose( fh );

    return ok;
}



static void
get_unit(char *unit,
         int *power,
         double ref)

{
    char *units[5] = {"pm","nm", "\xb5m", "mm", "m"};
    int i=0;
    ref = fabs (ref); //return no error if ref <= 0
    while ((log(ref) / log(10) < -11 + (i * 3)) && (++i < 4));
    strncpy(unit, units[i], 16);
    *power = 12 - i * 3;
}
