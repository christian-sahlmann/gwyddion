/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-intelliwave-esd">
 *   <comment>IntelliWave interferometric ESD data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="ESD IntelliWave"/>
 *   </magic>
 *   <glob pattern="*.esd"/>
 *   <glob pattern="*.ESD"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # IntelliWave interferometric ESD data
 * 0 string ESD\ IntelliWave IntelliWave interferometric ESD data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * IntelliWave ESD
 * .esd
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".esd"

#define MAGIC "ESD IntelliWave"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

enum {
    HEADER_SIZE = 1024,
    INFO_SIZE = 962,
    NAME_LEN = 50,
    N_IS_VALID = 50, /* the number of things that have is_valid[] flags */
};

/*
 * Data types:
 * 1D float: abberations, statistics
 * 2D float: OPD (surface), wrapped phase, modulation
 * 2D uchar: mask
 * 3D float: slope, curvature
 * 3D uchar: fringes
 */

/* This corresponds to indices in file_offsets[].  Values beyond 50 exist but
 * the documentation is silent about them.  */
typedef enum {
    INTWAVE_DATA_OPD = 0, /* surface map */
    INTWAVE_DATA_REF = 1, /* reference map */
    INTWAVE_DATA_WPHASE = 2, /* wrapped phase map */
    INTWAVE_DATA_MODULATION = 3, /* modulation map */
    INTWAVE_DATA_USER_MASK = 4, /* user mask */
    INTWAVE_DATA_INVALID_MASK = 5, /* invalid pixel mask */
    INTWAVE_DATA_STATISTICS = 6, /* surface statistics */
    INTWAVE_DATA_ABERRATIONS = 7, /* Zernike aberrations */
    INTWAVE_DATA_RMS_ORDER = 8, /* Zernike RMS Fit */
    INTWAVE_DATA_RMS_TERM = 9, /* N/A */
    INTWAVE_DATA_FRINGE = 10, /* interferogram data */
    INTWAVE_DATA_FRINGE_REF = 11, /* fringe reference */
    INTWAVE_DATA_MOD_STATISTICS = 12, /* modulation statistics */
    INTWAVE_DATA_FRINGE_STATISTICS = 13, /* interferogram statistics */
    INTWAVE_DATA_PHASESTEP_STATISTICS = 14, /* phase-step statistics */
    INTWAVE_DATA_PHASESTEP = 15, /* phase-step map */
    INTWAVE_DATA_ANAL_MASK = 16, /* analysis mask */
    INTWAVE_DATA_CODEV_OPD = 17, /* N/A */
    INTWAVE_DATA_CONFIG = 18, /* N/A */
    INTWAVE_DATA_DIFFRACTION = 19, /* diffraction FFT, PSF, or MTF */
    INTWAVE_DATA_FIDUCIAL = 20, /* fiducials */
    INTWAVE_DATA_ISLAND = 21, /* N/A */
    INTWAVE_DATA_CURVE_STATISTICS = 22, /* surface slope map */
    INTWAVE_DATA_SLOPE_STATISTICS = 23, /* surface slope statistics */
    INTWAVE_DATA_MASK_OBJECTS = 24, /* mask objects */
    INTWAVE_DATA_QC = 25, /* ?? */
    INTWAVE_DATA_PSF = 26, /* point spread function */
    INTWAVE_DATA_MTF = 27, /* modulation transfer function */
    INTWAVE_DATA_FRINGE_FFT = 28, /* FFT of interferogram */
    INTWAVE_DATA_UNWRAP = 29, /* unwrap error map */
    INTWAVE_DATA_UNWRAP_STATISTICS = 30, /* unwrap error map statistics */
    INTWAVE_DATA_NULL = 31, /* last one for bit testing */
    INTWAVE_DATA_SNAP = 32, /* N/A */
    INTWAVE_DATA_VIDEO = 33, /* N/A */
    INTWAVE_DATA_MOVIE = 34, /* valid when nFrames > 1 */
    INTWAVE_DATA_XSLOPE = 35, /* slope in the x direction */
    INTWAVE_DATA_YSLOPE = 36, /* slope in the y direction */
    INTWAVE_DATA_ZSLOPE = 37, /* slope in the z direction */
    INTWAVE_DATA_XCURVE = 38, /* curvature in the x direction */
    INTWAVE_DATA_YCURVE = 39, /* curvature in the y direction */
    INTWAVE_DATA_ZCURVE = 40, /* curvature in the z direction */
    INTWAVE_DATA_REF1 = 41, /* OPD buffer 1 */
    INTWAVE_DATA_REF2 = 42, /* OPD buffer 2 */
    INTWAVE_DATA_REF3 = 43, /* OPD buffer 3 */
    INTWAVE_DATA_REF4 = 44, /* OPD buffer 4 */
    INTWAVE_DATA_XSTRESS = 45, /* stress in the x direction */
    INTWAVE_DATA_YSTRESS = 46, /* stress in the y direction */
    INTWAVE_DATA_ZSTRESS = 47, /* stress in the z direction */
    INTWAVE_DATA_DISP_MASK = 48, /* N/A */
    INTWAVE_DATA_UNDO = 49, /* undo */
    INTWAVE_DATA_SAMPLE_INFO = 78, /* sample info structure */
} IntWaveDataIndex;

typedef struct {
    gchar name[NAME_LEN];
    gchar version[NAME_LEN];
    guint32 reserved1;
    guint32 file_offsets[100];
    guint n_data_sets;
    gchar reserved2[516];
} IntWaveFileHeader;

/* XXX: The alignmentN bytes are not specified in the documentation.  However,
 * they are in a place where a compiler would reasonably use alignment.  So
 * there is that... */
typedef struct {
    gchar name[NAME_LEN];    /* ESD file identifier */
    gchar alignment1[2];     /* XXX */
    guint32 type;
    guint32 nx, ny, nz;      /* nz = nx*ny = total points */
    guint32 nframes;         /* number of interferograms (if present) */
    gdouble wavelength;
    gdouble waves_per_fringe;
    gdouble aspect_ratio;
    gdouble invalid_value;             /* invalid value for all data types */
    gint32  is_valid[N_IS_VALID];      /* validation flags */
    gint32  xmin, xmax, xrange, xo;    /* aperture area of valid data */
    gint32  ymin, ymax, yrange, yo;    /* aperture area of valid data */
    gchar  reserved1[476];
    gchar fringe_time[NAME_LEN];       /* time acquired */
    gchar surface_time[NAME_LEN];      /* time processed */
    gdouble phase_shift;    /* phase shift between interferograms in degrees */
    gint reserved2;
    gint reserved3;
    gint max_fringe_intensity; /* maximum fringe intensity for camera used */
    gchar  wphase; /* wrapped phase map exists without interferogram data? */
    gchar  fringe_background;    /* the intensity background was removed? */
    gchar  fringe_type;     /* the interferograms are slope fringes? */
    gchar  future1;
    gchar  slope_dir;       /* 0 for horizontal slope, 1 for vertical slope */
    gchar  slope_flag;      /* reserved */
    gdouble x_slope_scale;  /* scale factor for horizontal slope */
    gdouble y_slope_scale;  /* scale factor for vertical slope */
    gdouble shear_dist;     /* normalized percentage of aperture */
    gchar reserved4[16];
    gint use_fringe_ordering;    /* interferograms are to be reordered? */
    gchar fo[20];                /* ordering of the interferograms */
} IntWaveFileInfo;

typedef struct {
    IntWaveFileHeader header;
    IntWaveFileInfo info;
} IntWaveFile;

static gboolean      module_register (void);
static gint          intw_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* intw_load       (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static const guchar* intw_read_header(const guchar *p,
                                      IntWaveFile *intwfile);
static const guchar* intw_read_info  (const guchar *p,
                                      IntWaveFile *intwfile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports IntelliWave inteferometric ESD data."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("intelliwave",
                           N_("IntelliWave inteferometric data (.esd)"),
                           (GwyFileDetectFunc)&intw_detect,
                           (GwyFileLoadFunc)&intw_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
intw_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size > HEADER_SIZE + INFO_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
intw_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    IntWaveFile intwfile;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < HEADER_SIZE + INFO_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;
    p = intw_read_header(p, &intwfile);
    g_assert(p - buffer == HEADER_SIZE);
    p = intw_read_info(p, &intwfile);
    g_assert(p - buffer == HEADER_SIZE + INFO_SIZE);

    gwy_file_abandon_contents(buffer, size, NULL);
    err_NO_DATA(error);

    //gwy_file_channel_import_log_add(container, 0, NULL, filename);

    return container;
}

static const guchar*
intw_read_header(const guchar *p,
                 IntWaveFile *intwfile)
{
    IntWaveFileHeader *header = &intwfile->header;
    guint i;

    get_CHARARRAY0(header->name, &p);
    gwy_debug("name %s", header->name);
    get_CHARARRAY0(header->version, &p);
    gwy_debug("version %s", header->version);
    header->reserved1 = gwy_get_guint32_le(&p);
    for (i = 0; i < G_N_ELEMENTS(header->file_offsets); i++) {
        header->file_offsets[i] = gwy_get_guint32_le(&p);
        if (header->file_offsets[i]) {
            gwy_debug("offset[%u] 0x%08x", i, header->file_offsets[i]);
        }
    }
    header->n_data_sets = gwy_get_guint32_le(&p);
    gwy_debug("n_data_sets %u", header->n_data_sets);
    get_CHARARRAY(header->reserved2, &p);

    return p;
}

static const guchar*
intw_read_info(const guchar *p,
               IntWaveFile *intwfile)
{
    IntWaveFileInfo *info = &intwfile->info;
    guint i;

    get_CHARARRAY0(info->name, &p);
    gwy_debug("name %s", info->name);
    get_CHARARRAY(info->alignment1, &p);
    info->type = gwy_get_guint32_le(&p);
    gwy_debug("type %u", info->type);
    info->nx = gwy_get_guint32_le(&p);
    info->ny = gwy_get_guint32_le(&p);
    info->nz = gwy_get_guint32_le(&p);
    gwy_debug("nx %u, ny %u, nz %u", info->nx, info->ny, info->nz);
    info->nframes = gwy_get_guint32_le(&p);
    info->wavelength = gwy_get_gfloat_le(&p);
    info->waves_per_fringe = gwy_get_gfloat_le(&p);
    info->aspect_ratio = gwy_get_gfloat_le(&p);
    info->invalid_value = gwy_get_gfloat_le(&p);
    for (i = 0; i < G_N_ELEMENTS(info->is_valid); i++) {
        info->is_valid[i] = gwy_get_guint32_le(&p);
        if (info->is_valid[i]) {
            gwy_debug("is_valid[%u] %u", i, info->is_valid[i]);
        }
    }
    info->xmin = gwy_get_guint32_le(&p);
    info->xmax = gwy_get_guint32_le(&p);
    info->xrange = gwy_get_guint32_le(&p);
    info->xo = gwy_get_guint32_le(&p);
    info->ymin = gwy_get_guint32_le(&p);
    info->ymax = gwy_get_guint32_le(&p);
    info->yrange = gwy_get_guint32_le(&p);
    info->yo = gwy_get_guint32_le(&p);
    get_CHARARRAY(info->reserved1, &p);
    get_CHARARRAY0(info->fringe_time, &p);
    gwy_debug("fringe_time %s", info->fringe_time);
    get_CHARARRAY0(info->surface_time, &p);
    gwy_debug("surface_time %s", info->surface_time);
    info->phase_shift = gwy_get_gfloat_le(&p);
    info->reserved2 = gwy_get_gint16_le(&p);
    info->reserved3 = gwy_get_gint16_le(&p);
    info->max_fringe_intensity = gwy_get_gint16_le(&p);
    info->wphase = *(p++);
    info->fringe_background = *(p++);
    info->fringe_type = *(p++);
    info->future1 = *(p++);
    info->slope_dir = *(p++);
    info->slope_flag = *(p++);
    info->x_slope_scale = gwy_get_gfloat_le(&p);
    info->y_slope_scale = gwy_get_gfloat_le(&p);
    gwy_debug("slope scales %g %g", info->x_slope_scale, info->y_slope_scale);
    info->shear_dist = gwy_get_gfloat_le(&p);
    get_CHARARRAY(info->reserved4, &p);
    info->use_fringe_ordering = gwy_get_gint16_le(&p);
    get_CHARARRAY(info->fo, &p);

    return p;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
