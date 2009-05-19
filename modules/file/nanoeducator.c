/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/* FIXME: What about .spm and .stm extensions?  Too generic? */
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanoeducator-spm">
 *   <comment>Nanoedu SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x19(C) STM Data File System \x00\x00\x00\x00"/>
 *   </magic>
 *   <glob pattern="*.mspm"/>
 *   <glob pattern="*.MSPM"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/spectra.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "\x19(C) STM Data File System \x00\x00\x00\x00"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION1 ".spm"
#define EXTENSION2 ".mspm"
#define EXTENSION3 ".stm"

#define Nanometer 1e-9
#define NanoAmpere 1e-9

enum {
    NANOEDU_IDENT_SIZE = 29,
    NANOEDU_LABEL_LENGTH = 20,
    NANOEDU_COMMENT_LENGTH = 60,
    NANOEDU_HEADER_SIZE = (1 + NANOEDU_IDENT_SIZE) + 3*2 + 2*1 + 2 + 4
        + 5*(2 + 2 + 4),
    NANOEDU_PARAMS_SIZE = (6*2 + 2*(1 + NANOEDU_LABEL_LENGTH) + 1 + 1)
        + (1 + 1 + 2*1 + 2*2 + 2 + 2)
        + (5*4)
        + (1 + 4 + 2*4 + 2 + 1 + 1 + 2 + 1)
        + (3*4 + 4 + 2*4 + 4 + 4)
        + (2 + 2*2 + 2*2 + 2 + 4)
        + (2*2 + 2 + 2*4 + 4 + 4 + 4 + 4 + 2*2 + 2*4)
        + (2 + 2 + 1 + 3*1)
        + (2*1 + 2*2 + 2*4)
        + (8*(1 + NANOEDU_COMMENT_LENGTH)),
};

/* XXX: Not all are used. */
typedef enum {
    NANOEDU_TOPOGRAPHY       = 0,
    NANOEDU_WORK_FORCE       = 1,
    NANOEDU_BACK_PASS        = 2,
    NANOEDU_PHASE            = 3,
    NANOEDU_UAM              = 4,
    NANOEDU_SPECTRA          = 5,
    NANOEDU_LITHO            = 6,
    NANOEDU_CURRENT_STM      = 7,
    NANOEDU_FAST_SCAN        = 8,
    NANOEDU_TOPO_ERROR       = 9,
    NANOEDU_FAST_SCAN_PHASE  = 10,
    NANOEDU_SCANNER_TRAINING = 11,
    NANOEDU_SENS_CORRECTION  = 12,
} NanoeduAquiAddType;

typedef struct {
    /* magic header, stored as a Pascal string */
    gchar ident[NANOEDU_IDENT_SIZE+1];
    guint version;
    guint flags;    /* nonzero if SPM file have been modified */
    /* record for the header */
    guint num_rec;
    guint bdata;
    guint bhead;
    guint head_size;
    guint head_offset;    /* offset of header data */
    /* record for the topography */
    gint topo_nx;
    gint topo_ny;
    gint topo_offset;    /* offset of topography data */
    /* record for ADDSURF */
    gint addsurf_ny;
    gint addsurf_nx;
    gint addsurf_offset;
    /* record for SPS points */
    gint point_nx;
    gint point_ny;
    gint point_offset;    /* offset of x,y for spectra */
    /* record for SPS data */
    gint spec_nx;
    gint spec_ny;
    gint spec_offset;    /* offset of spectra data */
    /* record for CVC */
    gint cvc_ny;
    gint cvc_nx;
    gint cvc_offset;
} NanoeduFileHeader;

typedef struct {
    guint year, month, day, hour, minute, second;    /* of scan */
    gchar material[NANOEDU_LABEL_LENGTH+1];    /* user comment about the sample material */
    gchar scanner_name[NANOEDU_LABEL_LENGTH+1];
    guint temperature;    /* unused */
    gboolean linear;
    /* data types */
    gboolean aqui_topo;    /* FALSE for only AQUIADD data */
    NanoeduAquiAddType aqui_add;
    gboolean aqui_spectr;
    gboolean aqui_cvc;
    gint topo_nx, topo_ny;    /* rectangular raster */
    gint n_spectra_lines;    /* number of spectra or CVC lines */
    gint n_spectrum_points;    /* points per line */
    /* scan parameters */
    gdouble xy_step;    /* in nanometers */
    gdouble scan_rate;    /* in nm/s */
    gdouble scan_voltage;    /* in mV */
    gdouble scan_current;    /* in nA */
    /* scan regimes */
    guint probe_type;    /* STM, SFM: FIXME */
    gdouble amp_zgain;    /* R Z-gain or F Z-gain, depending on z_tune */
    gdouble x_offset;    /* in nm */
    gdouble y_offset;    /* in nm */
    gint set_point;    /* in % */
    guint path_mode;    /* 0: X+; 1: Y+; 2-multi (unused) */
    guint reserved_scan1;
    gint reserved_scan2;
    guint reserved_scan3;
    /* hardware coefficients */
    gdouble sens_x, sens_y, sens_z;    /* nm/V */
    gdouble discr_z_mvolt;    /* mV/discrete Z */
    gdouble gain_x, gain_y;
    gdouble nA_D;    /* coefficient current in nA to discrete */
    gdouble V_D;    /* coefficient voltage in V to discrete */
    /* work function parameters */
    gint amp_modulation;
    guint sd_gain_fm;
    guint sd_gain_am;
    guint res_freq_r;
    guint res_freq_f;
    gint f0;    /* f0 in Hz */
    gdouble ampl_suppress;    /* amplitude suppression SFM */
    /* spectroscopy parameters */
    gint n_of_steps_x, n_of_steps_y;   /* num of spectra in x and y direction */
    gint n_of_averaging;    /* number of averaging */
    gdouble spec_voltage_start, spec_voltage_final;    /* in mV */
    gdouble time_spec_point;    /* in ms */
    gdouble spec_modulation;    /* modulation amplitude in mV */
    gdouble spec_detector_coeff;    /* synchrodetector coefficient */
    gdouble resistance;    /* in Ohm */
    gint reserved_spec1, reserved_spec2;
    gdouble reserved_spec3, reserved_spec4;
    /* spectroscopy regimes */
    gint cvc_type;
    gint spectroscopy_type;
    gboolean const_current;
    gboolean reserved_type1, reserved_type2, reserved_type3;
    /* reserved */
    gboolean reserved_bool1, reserved_bool2;
    gint reserved_int1, reserved_int2;
    gdouble reserved_float1, reserved_float2;
    /* comments */
    gchar comment1[NANOEDU_COMMENT_LENGTH+1];
    gchar comment2[NANOEDU_COMMENT_LENGTH+1];
    gchar comment3[NANOEDU_COMMENT_LENGTH+1];
    gchar comment4[NANOEDU_COMMENT_LENGTH+1];
    gchar comment5[NANOEDU_COMMENT_LENGTH+1];
    gchar comment6[NANOEDU_COMMENT_LENGTH+1];
    gchar comment7[NANOEDU_COMMENT_LENGTH+1];
    gchar comment8[NANOEDU_COMMENT_LENGTH+1];
} NanoeduParameterHeader;

static gboolean       module_register        (void);
static gint           nanoedu_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*  nanoedu_load           (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gsize          nanoedu_read_header    (const guchar *buffer,
                                              gsize size,
                                              NanoeduFileHeader *header,
                                              GError **error);
static gsize          nanoedu_read_parameters(const guchar *buffer,
                                              gsize size,
                                              NanoeduParameterHeader *params,
                                              GError **error);
static GwyDataField*  nanoedu_read_data_field(const guchar *buffer,
                                              gsize size,
                                              gint xres,
                                              gint yres,
                                              gdouble xreal,
                                              gdouble yreal,
                                              const gchar *xyunits,
                                              const char *zunits,
                                              gdouble q,
                                              GError **error);
static GwyGraphModel* nanoedu_read_graph     (const guchar *buffer,
                                              gsize size,
                                              gint res,
                                              gint ncurves,
                                              gdouble real,
                                              const gchar *xunits,
                                              const char *yunits,
                                              gdouble q,
                                              GError **error);
static GwySpectra*    nanoedu_read_fd_spectra(const guchar *pos_buffer,
                                              gsize pos_size,
                                              const guchar *data_buffer,
                                              gsize data_size,
                                              gint version,
                                              gint nspectra,
                                              gint res,
                                              gdouble xy_step,
                                              gdouble yreal,
                                              gdouble xscale,
                                              gdouble yscale,
                                              GError **error);
static GwySpectra*    nanoedu_read_iv_spectra(const guchar *pos_buffer,
                                              gsize pos_size,
                                              const guchar *data_buffer,
                                              gsize data_size,
                                              gint version,
                                              gint nspectra,
                                              gint res,
                                              gdouble yreal,
                                              gdouble xscale,
                                              gdouble yscale,
                                              gdouble vscale,
                                              GError **error);
static GwySpectra*    nanoedu_read_iz_spectra(const guchar *pos_buffer,
                                              gsize pos_size,
                                              const guchar *data_buffer,
                                              gsize data_size,
                                              gint version,
                                              gint nspectra,
                                              gint res,
                                              gdouble xy_step,
                                              gdouble yreal,
                                              gdouble xscale,
                                              gdouble yscale,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanoeducator data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoeducator",
                           N_("Nanoeducator files (.mspm, .spm, .stm)"),
                           (GwyFileDetectFunc)&nanoedu_detect,
                           (GwyFileLoadFunc)&nanoedu_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanoedu_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION1)
                || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION2)
                || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION3))
               ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
nanoedu_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    NanoeduFileHeader header;
    NanoeduParameterHeader params;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyGraphModel *gmodel = NULL;
    GwySpectra *spectra = NULL;
    gdouble scale, q, qx, qy;
    const gchar *units, *title;
    guint nobjects = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    p = buffer;

    if (!(len = nanoedu_read_header(p, size, &header, error)))
        goto finish;
    p += len;

    if (header.version < 11) {
        err_UNSUPPORTED(error, _("format version"));
        goto finish;
    }

    if (!(len = nanoedu_read_parameters(p, size - (p - buffer), &params,
                                        error)))
        goto finish;
    p += len;

    container = gwy_container_new();

    scale = Nanometer * params.xy_step;
    /* Version 12+ */
    q = 1e-3 * params.sens_z * params.amp_zgain * params.discr_z_mvolt;
    /* Version 11. */
    if (header.version == 11 || !q)
        q = 1.0;

    /* The basic topography data, they need not to be always present though. */
    if (params.aqui_topo && header.topo_nx && header.topo_ny
        && !(params.aqui_add == NANOEDU_SCANNER_TRAINING)) {
        if (err_DIMENSION(error, header.topo_nx)
            || err_DIMENSION(error, header.topo_ny))
            goto finish;
        if (header.topo_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Image data starts past the end of file."));
            goto finish;
        }
        if (params.topo_nx != header.topo_nx)
            g_warning("params.topo_nx (%d) != header.topo_nx (%d), "
                      "choosing header", params.topo_nx, header.topo_nx);
        if (params.topo_ny != header.topo_ny)
            g_warning("params.topo_ny (%d) != header.topo_ny (%d), "
                      "choosing header", params.topo_ny, header.topo_ny);

        dfield = nanoedu_read_data_field(buffer + header.topo_offset,
                                         size - header.topo_offset,
                                         header.topo_nx,
                                         header.topo_ny,
                                         scale*header.topo_nx,
                                         scale*header.topo_ny,
                                         "m", "m", q*Nanometer, error);
        if (!dfield)
            goto finish;

        gwy_container_set_object_by_name(container, "/0/data", dfield);
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Topography"));
        g_object_unref(dfield);
        nobjects++;
    }

    /* Additonal, but in fact alternative, data: one-line scans */
    if (header.topo_nx && header.topo_ny
        && params.aqui_add == NANOEDU_SCANNER_TRAINING) {
        if (err_DIMENSION(error, header.topo_nx)
            || err_DIMENSION(error, header.topo_ny))
            goto finish;
        if (header.topo_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Image data starts past the end of file."));
            goto finish;
        }

        /* Version 12+ */
        q = 1e-3 * params.sens_z * params.amp_zgain * params.discr_z_mvolt;
        /* Version 11. */
        if (header.version == 11 || !q)
            q = 1.0;
        q *= Nanometer;
        units = "m";

        gmodel = nanoedu_read_graph(buffer + header.topo_offset,
                                    size - header.topo_offset,
                                    header.topo_nx, header.topo_ny,
                                    scale*header.topo_nx,
                                    "m", units, q, error);
        if (!gmodel)
            goto finish;

        g_object_set(gmodel,
                     "title",
                     params.path_mode ? "Scanner Training (Y+)"
                                      : "Scanner Training (X+)",
                     NULL);
        gwy_container_set_object_by_name(container, "/0/graph/graph/1", gmodel);
        g_object_unref(gmodel);
        nobjects++;

        /* This was already addsurf, so do not attempt to read it again. */
        goto finish;
    }

    /* Additonal data: spectra */
    if (params.aqui_spectr
        && params.n_spectra_lines && params.n_spectrum_points) {
        if (err_DIMENSION(error, params.n_spectra_lines)
            || err_DIMENSION(error, params.n_spectrum_points))
            goto finish;
        if (header.point_offset >= size || header.spec_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Spectra data starts past the end of file."));
            goto finish;
        }

        qx = 1e-3 * params.sens_x * params.gain_x * params.discr_z_mvolt;
        qy = 1e-3 * params.sens_y * params.gain_y * params.discr_z_mvolt;

        /* FIXME: This might be wrong, however, there does not seem to be any
         * other discriminating quantity in the headers. */
        if (params.probe_type == 0)
            spectra = nanoedu_read_fd_spectra(buffer + header.point_offset,
                                              size - header.point_offset,
                                              buffer + header.spec_offset,
                                              size - header.spec_offset,
                                              header.version,
                                              params.n_spectra_lines,
                                              params.n_spectrum_points,
                                              Nanometer*q, scale*header.topo_ny,
                                              Nanometer*qx, Nanometer*qy,
                                              error);
        else if (params.amp_modulation == 1)
            spectra = nanoedu_read_iz_spectra(buffer + header.point_offset,
                                              size - header.point_offset,
                                              buffer + header.spec_offset,
                                              size - header.spec_offset,
                                              header.version,
                                              params.n_spectra_lines,
                                              params.n_spectrum_points,
                                              Nanometer*q, scale*header.topo_ny,
                                              Nanometer*qx, Nanometer*qy,
                                              error);
        else
            spectra = nanoedu_read_iv_spectra(buffer + header.point_offset,
                                              size - header.point_offset,
                                              buffer + header.spec_offset,
                                              size - header.spec_offset,
                                              header.version,
                                              params.n_spectra_lines,
                                              params.n_spectrum_points,
                                              scale*header.topo_ny,
                                              Nanometer*qx, Nanometer*qy,
                                              1e-3 * params.discr_z_mvolt,
                                              error);

        if (!spectra)
            goto finish;

        gwy_container_set_object_by_name(container, "/sps/0", spectra);
        g_object_unref(spectra);
        nobjects++;
    }

    /* Additonal data: two-dimensional data. */
    /* This seems to be the only way to recognize whether addsurf is present
     * because addsurf type 0 is a valid type. */
    if (header.addsurf_nx && header.addsurf_ny >= 1) {
        if (err_DIMENSION(error, header.addsurf_nx)
            || err_DIMENSION(error, header.addsurf_ny))
            goto finish;
        if (header.addsurf_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Image data starts past the end of file."));
            goto finish;
        }

        switch (params.aqui_add) {
            case NANOEDU_TOPOGRAPHY:
            case NANOEDU_BACK_PASS:
            case NANOEDU_LITHO:
            case NANOEDU_SCANNER_TRAINING:
            /* Version 12+ */
            q = 1e-3 * params.sens_z * params.amp_zgain * params.discr_z_mvolt;
            /* Version 11. */
            if (header.version == 11 || !q)
                q = 1.0;
            q *= Nanometer;
            units = "m";
            break;

            case NANOEDU_PHASE:
            case NANOEDU_FAST_SCAN_PHASE:
            q = 1.0;
            units = "";
            break;

            case NANOEDU_UAM:
            q = 1.0/params.V_D;
            units = "V";
            break;

            case NANOEDU_WORK_FORCE:
            case NANOEDU_CURRENT_STM:
            case NANOEDU_FAST_SCAN:
            q = NanoAmpere/params.nA_D;
            units = "A";
            break;

            default:
            g_warning("Data conversion factor for type %u is not known!",
                      params.aqui_add);
            q = 1.0;
            units = "";
            break;
        }

        dfield = nanoedu_read_data_field(buffer + header.addsurf_offset,
                                          size - header.addsurf_offset,
                                          header.addsurf_nx,
                                          header.addsurf_ny,
                                          scale*header.addsurf_nx,
                                          scale*header.addsurf_ny,
                                          "m", units, q, error);
        if (!dfield)
            goto finish;

        gwy_container_set_object_by_name(container, "/1/data", dfield);
        title = gwy_enuml_to_string(params.aqui_add,
                                    "Topography", NANOEDU_TOPOGRAPHY,
                                    "Work Force",  NANOEDU_WORK_FORCE,
                                    "Back Pass", NANOEDU_BACK_PASS,
                                    "Phase", NANOEDU_PHASE,
                                    "UAM", NANOEDU_UAM,
                                    /* XXX: Should not happen... */
                                    "Spectra", NANOEDU_SPECTRA,
                                    "Litho", NANOEDU_LITHO,
                                    "Current STM", NANOEDU_CURRENT_STM,
                                    "Fast Scan", NANOEDU_FAST_SCAN,
                                    "Topography Error", NANOEDU_TOPO_ERROR,
                                    "Fast Scan Phase", NANOEDU_FAST_SCAN_PHASE,
                                    "Scanner Training", NANOEDU_SCANNER_TRAINING,
                                    "Sens. Correction", NANOEDU_SENS_CORRECTION,
                                    NULL);
        if (title && *title)
            gwy_container_set_string_by_name(container, "/1/data/title",
                                             g_strdup(title));
        g_object_unref(dfield);
        nobjects++;
    }

    if (!nobjects)
        err_NO_DATA(error);

finish:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!nobjects)
        gwy_object_unref(container);
    else
        g_clear_error(error);

    return container;
}

static gsize
nanoedu_read_header(const guchar *buffer,
                    gsize size,
                    NanoeduFileHeader *header,
                    GError **error)
{
    if (size < NANOEDU_HEADER_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        return 0;
    }

    /* identification */
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanoeducator");
        return 0;
    }
    memcpy(header->ident, buffer+1, NANOEDU_IDENT_SIZE);
    buffer += 1 + NANOEDU_IDENT_SIZE;

    /* header */
    header->version = gwy_get_guint16_le(&buffer);
    header->flags = gwy_get_guint16_le(&buffer);
    header->num_rec = gwy_get_guint16_le(&buffer);
    header->bdata = *(buffer++);
    header->bhead = *(buffer++);
    header->head_size = gwy_get_guint16_le(&buffer);
    header->head_offset = gwy_get_gint32_le(&buffer);
    /* XXX: I do not like the dimensions being signed, but that's how the
     * format declares them. */
    header->topo_ny = gwy_get_gint16_le(&buffer);
    header->topo_nx = gwy_get_gint16_le(&buffer);
    header->topo_offset = gwy_get_gint32_le(&buffer);
    gwy_debug("topo_nx=%d, topo_ny=%d, topo_offset=0x%08x",
              header->topo_nx, header->topo_ny, header->topo_offset);
    header->addsurf_ny = gwy_get_gint16_le(&buffer);
    header->addsurf_nx = gwy_get_gint16_le(&buffer);
    header->addsurf_offset = gwy_get_gint32_le(&buffer);
    gwy_debug("addsurf_nx=%d, addsurf_ny=%d, addsurf_offset=0x%08x",
              header->addsurf_nx, header->addsurf_ny, header->addsurf_offset);
    header->point_ny = gwy_get_gint16_le(&buffer);
    header->point_nx = gwy_get_gint16_le(&buffer);
    header->point_offset = gwy_get_gint32_le(&buffer);
    gwy_debug("point_nx=%d, point_ny=%d, point_offset=0x%08x",
              header->point_nx, header->point_ny, header->point_offset);
    header->spec_ny = gwy_get_gint16_le(&buffer);
    header->spec_nx = gwy_get_gint16_le(&buffer);
    header->spec_offset = gwy_get_gint32_le(&buffer);
    gwy_debug("spec_nx=%d, spec_ny=%d, spec_offset=0x%08x",
              header->spec_nx, header->spec_ny, header->spec_offset);
    header->cvc_ny = gwy_get_gint16_le(&buffer);
    header->cvc_nx = gwy_get_gint16_le(&buffer);
    header->cvc_offset = gwy_get_gint32_le(&buffer);
    gwy_debug("cvc_nx=%d, cvc_ny=%d, cvc_offset=0x%08x",
              header->cvc_nx, header->cvc_ny, header->cvc_offset);

    return NANOEDU_HEADER_SIZE;
}

static gsize
nanoedu_read_parameters(const guchar *buffer,
                        gsize size,
                        NanoeduParameterHeader *params,
                        GError **error)
{
    if (size < NANOEDU_PARAMS_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Parameter header is truncated"));
        return 0;
    }

    params->year = gwy_get_guint16_le(&buffer);
    params->month = gwy_get_guint16_le(&buffer);
    params->day = gwy_get_guint16_le(&buffer);
    params->hour = gwy_get_guint16_le(&buffer);
    params->minute = gwy_get_guint16_le(&buffer);
    params->second = gwy_get_guint16_le(&buffer);
    gwy_debug("%04u-%02u-%02u %02u:%02u:%02u",
              params->year, params->month, params->day,
              params->hour, params->minute, params->second);
    get_PASCAL_CHARS0(params->material, &buffer, NANOEDU_LABEL_LENGTH);
    get_PASCAL_CHARS0(params->scanner_name, &buffer, NANOEDU_LABEL_LENGTH);
    gwy_debug("material=<%s>, scanner=<%s>",
              params->material, params->scanner_name);
    params->temperature = *(buffer++);
    params->linear = gwy_get_gboolean8(&buffer);

    params->aqui_topo = gwy_get_gboolean8(&buffer);
    params->aqui_add = *(buffer++);
    params->aqui_spectr = gwy_get_gboolean8(&buffer);
    params->aqui_cvc = gwy_get_gboolean8(&buffer);
    params->topo_nx = gwy_get_gint16_le(&buffer);
    params->topo_ny = gwy_get_gint16_le(&buffer);
    params->n_spectra_lines = gwy_get_gint16_le(&buffer);
    params->n_spectrum_points = gwy_get_gint16_le(&buffer);
    gwy_debug("topo: %d [%dx%d], spectr: %d [%dx%d], cvc: %d, aqui_add=%d",
              params->aqui_topo, params->topo_nx, params->topo_ny,
              params->aqui_spectr, params->n_spectra_lines, params->n_spectrum_points,
              params->aqui_cvc, params->aqui_add);

    params->xy_step = gwy_get_gfloat_le(&buffer);
    params->scan_rate = gwy_get_gfloat_le(&buffer);
    params->scan_voltage = gwy_get_gfloat_le(&buffer);
    params->scan_current = gwy_get_gfloat_le(&buffer);

    params->probe_type = *(buffer++);
    params->amp_zgain = gwy_get_gfloat_le(&buffer);
    params->x_offset = gwy_get_gfloat_le(&buffer);
    params->y_offset = gwy_get_gfloat_le(&buffer);
    params->set_point = gwy_get_gint16_le(&buffer);
    params->path_mode = *(buffer++);
    params->reserved_scan1 = *(buffer++);
    params->reserved_scan2 = gwy_get_gint16_le(&buffer);
    params->reserved_scan3 = *(buffer++);
    gwy_debug("amp_zgain=%g, xy_step=%g",
              params->amp_zgain, params->xy_step);

    params->sens_x = gwy_get_gfloat_le(&buffer);
    params->sens_y = gwy_get_gfloat_le(&buffer);
    params->sens_z = gwy_get_gfloat_le(&buffer);
    gwy_debug("sens_x=%g, sens_y=%g, sens_z=%g",
              params->sens_x, params->sens_y, params->sens_z);
    params->discr_z_mvolt = gwy_get_gfloat_le(&buffer);
    params->gain_x = gwy_get_gfloat_le(&buffer);
    params->gain_y = gwy_get_gfloat_le(&buffer);
    params->nA_D = gwy_get_gfloat_le(&buffer);
    params->V_D = gwy_get_gfloat_le(&buffer);
    gwy_debug("gain_x=%g, gain_y=%g, discr_z_mvolt=%g, nA_d=%g, V_D=%g",
              params->gain_x, params->gain_y, params->discr_z_mvolt,
              params->nA_D, params->V_D);

    params->amp_modulation = gwy_get_gint32_le(&buffer); // XXX
    params->sd_gain_fm = gwy_get_guint16_le(&buffer);
    params->sd_gain_am = gwy_get_guint16_le(&buffer);
    params->res_freq_r = gwy_get_guint16_le(&buffer);
    params->res_freq_f = gwy_get_guint16_le(&buffer);
    params->f0 = gwy_get_gint32_le(&buffer); // XXX
    params->ampl_suppress = gwy_get_gfloat_le(&buffer);
    gwy_debug("work func: %d (%u %u) (%u %u) %d %g",
              params->amp_modulation, params->sd_gain_fm, params->sd_gain_am,
              params->res_freq_r, params->res_freq_f, params->f0,
              params->ampl_suppress);

    params->n_of_steps_x = gwy_get_gint16_le(&buffer);
    params->n_of_steps_y = gwy_get_gint16_le(&buffer);
    params->n_of_averaging = gwy_get_gint16_le(&buffer);
    gwy_debug("n_of_steps_x=%d, n_of_steps_y=%d, n_of_averaging=%d",
              params->n_of_steps_x, params->n_of_steps_y,
              params->n_of_averaging);
    params->spec_voltage_start = gwy_get_gfloat_le(&buffer);
    params->spec_voltage_final = gwy_get_gfloat_le(&buffer);
    params->time_spec_point = gwy_get_gfloat_le(&buffer);
    params->spec_modulation = gwy_get_gfloat_le(&buffer);
    params->spec_detector_coeff = gwy_get_gfloat_le(&buffer);
    params->resistance = gwy_get_gfloat_le(&buffer);
    gwy_debug("spec_voltage=[%g,%g], time_spec_point=%g, spec_modulation=%g, "
              "spec_detector_coeff=%g, resistance=%g",
              params->spec_voltage_start, params->spec_voltage_final,
              params->time_spec_point, params->spec_modulation,
              params->spec_detector_coeff, params->resistance);
    params->reserved_spec1 = gwy_get_gint16_le(&buffer);
    params->reserved_spec2 = gwy_get_gint16_le(&buffer);
    params->reserved_spec3 = gwy_get_gfloat_le(&buffer);
    params->reserved_spec4 = gwy_get_gfloat_le(&buffer);

    params->cvc_type = gwy_get_gint16_le(&buffer);
    params->spectroscopy_type = gwy_get_gint16_le(&buffer);
    gwy_debug("spectroscopy_type=%d", params->spectroscopy_type);
    params->const_current = gwy_get_gboolean8(&buffer);
    params->reserved_type1 = gwy_get_gboolean8(&buffer);
    params->reserved_type2 = gwy_get_gboolean8(&buffer);
    params->reserved_type3 = gwy_get_gboolean8(&buffer);

    params->reserved_bool1 = gwy_get_gboolean8(&buffer);
    params->reserved_bool2 = gwy_get_gboolean8(&buffer);
    params->reserved_int1 = gwy_get_gint16_le(&buffer);
    params->reserved_int2 = gwy_get_gint16_le(&buffer);
    params->reserved_float1 = gwy_get_gfloat_le(&buffer);
    params->reserved_float2 = gwy_get_gfloat_le(&buffer);

    get_PASCAL_CHARS0(params->comment1, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment2, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment3, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment4, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment5, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment6, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment7, &buffer, NANOEDU_COMMENT_LENGTH);
    get_PASCAL_CHARS0(params->comment8, &buffer, NANOEDU_COMMENT_LENGTH);
    gwy_debug("comm: <%s> <%s> <%s> <%s> <%s> <%s> <%s> <%s>",
              params->comment1, params->comment2,
              params->comment3, params->comment4,
              params->comment5, params->comment6,
              params->comment7, params->comment8);

    return NANOEDU_PARAMS_SIZE;
}

static GwyDataField*
nanoedu_read_data_field(const guchar *buffer,
                        gsize size,
                        gint xres, gint yres,
                        gdouble xreal, gdouble yreal,
                        const gchar *xyunits, const char *zunits,
                        gdouble q,
                        GError **error)
{
    gint i, j;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data;
    const gint16 *d16 = (const gint16*)buffer;

    if (err_SIZE_MISMATCH(error, 2*xres*yres, size, FALSE))
        return NULL;

    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++) {
        gdouble *row = data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++) {
            gint16 v = d16[i*xres + j];
            row[j] = q*GINT16_FROM_LE(v);
        }
    }

    siunit = gwy_si_unit_new(xyunits);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new(zunits);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

static GwyGraphModel*
nanoedu_read_graph(const guchar *buffer,
                   gsize size,
                   gint res, gint ncurves, gdouble real,
                   const gchar *xunits, const char *yunits,
                   gdouble q,
                   GError **error)
{
    gint i, j;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *siunitx, *siunity;
    gdouble *xdata, *ydata;
    const gint16 *d16 = (const gint16*)buffer;
    GString *str;

    if (err_SIZE_MISMATCH(error, 2*res*ncurves, size, FALSE))
        return NULL;

    /* Use negated positive conditions to catch NaNs */
    if (!((real = fabs(real)) > 0)) {
        g_warning("Real size is 0.0, fixing to 1.0");
        real = 1.0;
    }

    siunitx = gwy_si_unit_new(xunits);
    siunity = gwy_si_unit_new(yunits);
    gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL,
                          "si-unit-x", siunitx,
                          "si-unit-y", siunity,
                          NULL);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    xdata = g_new(gdouble, 2*res);
    ydata = xdata + res;
    str = g_string_new(NULL);

    for (i = 0; i < ncurves; i++) {
        for (j = 0; j < res; j++) {
            gint16 v = d16[i*res + j];
            xdata[j] = j*real/(res - 1);
            ydata[j] = q*GINT16_FROM_LE(v);
        }

        g_string_printf(str, _("Profile %d"), i);
        gcmodel = g_object_new(GWY_TYPE_GRAPH_CURVE_MODEL,
                               "description", str->str,
                               "mode", GWY_GRAPH_CURVE_LINE,
                               "color", gwy_graph_get_preset_color(i),
                               NULL);
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    g_string_free(str, TRUE);
    g_free(xdata);

    return gmodel;
}

static guint
check_spectra_size(guint version, guint nspectra,
                   const gint16 *p16, gsize pos_size,
                   GError **error)
{
    guint i, n, speccount, pointstep;

    /* In versions up to 13, there are only (X, Y) spectra positions.
     * In version 14+, there are (X, Y, N) triplets where N denotes the number
     * of spectra measured in the point.  Since the number of spectra can
     * differ from point to point in v14+, check if the counts sum up first. */
    if (version >= 14) {
        /* The maximum number of coodinates that can be there.
         * FIXME: This is too optimistic, pos_size can extend to the end of
         * the file. */
        pointstep = 3;
        n = pos_size/(2*pointstep);

        for (i = speccount = 0; i < n && speccount < nspectra; i++) {
            speccount += GINT16_FROM_LE(p16[pointstep*i + 2]);
            if (speccount >= nspectra)
                break;
        }
        if (speccount != nspectra) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("The number of spectra coordinates does "
                          "not match the number of spectra."));
            return 0;
        }
    }
    else {
        pointstep = 2;
        if (err_SIZE_MISMATCH(error, pointstep*2*nspectra, pos_size, FALSE))
            return 0;
    }

    return pointstep;
}

static GwyDataLine*
make_fd_spectrum(gint res, gdouble xy_step, const gint16 *d16, gboolean flip)
{
    GwyDataLine *dline;
    GwySIUnit *siunitx, *siunity;
    gdouble *data;
    gint j, amin;
    gdouble z0;

    dline = gwy_data_line_new(res, xy_step*res, FALSE);
    siunitx = gwy_si_unit_new("m");
    siunity = gwy_si_unit_new(NULL);
    gwy_data_line_set_si_unit_x(dline, siunitx);
    gwy_data_line_set_si_unit_y(dline, siunity);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    data = gwy_data_line_get_data(dline);
    amin = G_MAXINT;
    z0 = 1.0;
    /* XXX: The odd coordinates are abscissas.  We only use the zeroth for
     * setting the offset.  If they are not equidistant, though luck... */
    for (j = 0; j < res; j++) {
        gint v, a;

        if (flip) {
            v = d16[2*(res-1 - j)];
            a = d16[2*(res-1 - j) + 1];
        }
        else {
            v = d16[2*j];
            a = d16[2*j + 1];
        }
        data[j] = GINT16_FROM_LE(v);
        /* Find the abscissa closest to zero, the values should be divied
         * by the value at zero Z */
        a = GINT16_FROM_LE(a);
        if (abs(a) < abs(amin)) {
            amin = a;
            z0 = data[j];
        }
    }
    gwy_data_line_multiply(dline, 1.0/z0);
    gwy_data_line_set_offset(dline, xy_step*d16[flip ? 2*(res-1) + 1 : 1]);

    return dline;
}

static GwySpectra*
nanoedu_read_fd_spectra(const guchar *pos_buffer, gsize pos_size,
                        const guchar *data_buffer, gsize data_size,
                        gint version,
                        gint nspectra, gint res,
                        gdouble xy_step, gdouble yreal,
                        gdouble xscale, gdouble yscale,
                        GError **error)
{
    gint i, n, speccount, pointstep;
    GwySpectra *spectra;
    GwyDataLine *dline;
    GwySIUnit *siunit;
    const gint16 *p16 = (const gint16*)pos_buffer;
    const gint16 *d16 = (const gint16*)data_buffer;
    gdouble x, y;
    gdouble *data;

    if (!(pointstep = check_spectra_size(version, nspectra, p16, pos_size,
                                         error)))
        return NULL;

    if (err_SIZE_MISMATCH(error, 2*4*nspectra*res, data_size, FALSE))
        return NULL;

    /* Use negated positive conditions to catch NaNs */
    if (!((xy_step = fabs(xy_step)) > 0)) {
        g_warning("Real size is 0.0, fixing to 1.0");
        xy_step = 1.0;
    }

    spectra = gwy_spectra_new();
    siunit = gwy_si_unit_new("m");
    gwy_spectra_set_si_unit_xy(spectra, siunit);
    g_object_unref(siunit);
    gwy_spectra_set_title(spectra, _("F-D spectra"));

    /* For FD curves, there are always two spectra: forward and backward.
     * The backward one is really stored backwards, so we revert it upon
     * reading. */
    for (i = speccount = 0; speccount < nspectra; i++) {
        x = xscale*GINT16_FROM_LE(p16[pointstep*i]);
        y = yreal - yscale*GINT16_FROM_LE(p16[pointstep*i + 1]);
        n = (pointstep == 3) ? GINT16_FROM_LE(p16[pointstep*i + 2]) : 1;
        gwy_debug("FD spec%d [%g,%g] %dpts", i, x, y, n);

        while (n--) {
            /* Forward */
            dline = make_fd_spectrum(res, xy_step,
                                     d16 + 2*2*speccount*res, FALSE);
            data = gwy_data_line_get_data(dline);
            gwy_spectra_add_spectrum(spectra, dline, x, y);
            g_object_unref(dline);

            /* Backward */
            dline = make_fd_spectrum(res, xy_step,
                                     d16 + 2*2*speccount*res + 2*res, TRUE);
            gwy_spectra_add_spectrum(spectra, dline, x, y);
            g_object_unref(dline);

            speccount++;
        }
    }

    return spectra;
}

static GwyDataLine*
make_iv_spectrum(gint res, gdouble xy_step,
                 const gint16 *d16, gdouble q,
                 gboolean flip)
{
    GwyDataLine *dline;
    GwySIUnit *siunitx, *siunity;
    gdouble *data;
    gint j;

    dline = gwy_data_line_new(res, xy_step*res, FALSE);
    siunitx = gwy_si_unit_new("V");
    siunity = gwy_si_unit_new("A");
    gwy_data_line_set_si_unit_x(dline, siunitx);
    gwy_data_line_set_si_unit_y(dline, siunity);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    data = gwy_data_line_get_data(dline);
    /* XXX: The even coordinates are abscissas.  We only use the zeroth for
     * setting the offset.  If they are not equidistant, though luck... */
    for (j = 0; j < res; j++) {
        gint v;

        if (flip)
            v = d16[2*(res-1 - j) + 1];
        else
            v = d16[2*j + 1];

        data[j] = q*GINT16_FROM_LE(v);
    }
    gwy_data_line_set_offset(dline, xy_step*d16[flip ? 2*(res-1) : 0]);

    return dline;
}

static GwySpectra*
nanoedu_read_iv_spectra(const guchar *pos_buffer, gsize pos_size,
                        const guchar *data_buffer, gsize data_size,
                        gint version,
                        gint nspectra, gint res,
                        gdouble yreal,
                        gdouble xscale, gdouble yscale,
                        gdouble vscale,
                        GError **error)
{
    gint i, n, speccount, pointstep;
    GwySpectra *spectra;
    GwyDataLine *dline;
    GwySIUnit *siunit;
    const gint16 *p16 = (const gint16*)pos_buffer;
    const gint16 *d16 = (const gint16*)data_buffer;
    gdouble x, y;
    gdouble *data;

    if (!(pointstep = check_spectra_size(version, nspectra, p16, pos_size,
                                         error)))
        return NULL;

    if (err_SIZE_MISMATCH(error, 2*4*nspectra*res, data_size, FALSE))
        return NULL;

    spectra = gwy_spectra_new();
    siunit = gwy_si_unit_new("m");
    gwy_spectra_set_si_unit_xy(spectra, siunit);
    g_object_unref(siunit);
    gwy_spectra_set_title(spectra, _("I-V spectra"));

    /* For IV curves, there are always two spectra: forward and backward.
     * The backward one is really stored backwards, so we revert it upon
     * reading. */
    for (i = speccount = 0; speccount < nspectra; i++) {
        x = xscale*GINT16_FROM_LE(p16[pointstep*i]);
        y = yreal - yscale*GINT16_FROM_LE(p16[pointstep*i + 1]);
        n = (pointstep == 3) ? GINT16_FROM_LE(p16[pointstep*i + 2]) : 1;
        gwy_debug("IV spec%d [%g,%g] %dpts", i, x, y, n);

        while (n--) {
            /* Forward */
            dline = make_iv_spectrum(res, vscale,
                                     d16 + 2*2*speccount*res, 1e-12,
                                     FALSE);
            data = gwy_data_line_get_data(dline);
            gwy_spectra_add_spectrum(spectra, dline, x, y);
            g_object_unref(dline);

            /* Backward */
            dline = make_iv_spectrum(res, vscale,
                                     d16 + 2*2*speccount*res + 2*res, 1e-12,
                                     FALSE);
            gwy_spectra_add_spectrum(spectra, dline, x, y);
            g_object_unref(dline);

            speccount++;
        }
    }

    return spectra;
}

static GwyDataLine*
make_iz_spectrum(gint res, gdouble xy_step,
                 const gint16 *d16, gdouble q,
                 gboolean flip)
{
    GwyDataLine *dline;
    GwySIUnit *siunitx, *siunity;
    gdouble *data;
    gint j;

    dline = gwy_data_line_new(res, xy_step*res, FALSE);
    siunitx = gwy_si_unit_new("m");
    siunity = gwy_si_unit_new("A");
    gwy_data_line_set_si_unit_x(dline, siunitx);
    gwy_data_line_set_si_unit_y(dline, siunity);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    data = gwy_data_line_get_data(dline);
    /* XXX: The odd coordinates are abscissas.  We only use the zeroth for
     * setting the offset.  If they are not equidistant, though luck... */
    for (j = 0; j < res; j++) {
        gint v;

        if (flip)
            v = d16[2*(res-1 - j)];
        else
            v = d16[2*j];

        data[j] = q*GINT16_FROM_LE(v);
    }
    gwy_data_line_set_offset(dline, xy_step*d16[flip ? 2*(res-1) + 1 : 1]);

    return dline;
}

static GwySpectra*
nanoedu_read_iz_spectra(const guchar *pos_buffer, gsize pos_size,
                        const guchar *data_buffer, gsize data_size,
                        gint version,
                        gint nspectra, gint res,
                        gdouble xy_step, gdouble yreal,
                        gdouble xscale, gdouble yscale,
                        GError **error)
{
    gint i, n, speccount, pointstep;
    GwySpectra *spectra;
    GwyDataLine *dline;
    GwySIUnit *siunit;
    const gint16 *p16 = (const gint16*)pos_buffer;
    const gint16 *d16 = (const gint16*)data_buffer;
    gdouble x, y;
    gdouble *data;

    if (!(pointstep = check_spectra_size(version, nspectra, p16, pos_size,
                                         error)))
        return NULL;

    if (err_SIZE_MISMATCH(error, 4*nspectra*res, data_size, FALSE))
        return NULL;

    spectra = gwy_spectra_new();
    siunit = gwy_si_unit_new("m");
    gwy_spectra_set_si_unit_xy(spectra, siunit);
    g_object_unref(siunit);
    gwy_spectra_set_title(spectra, _("I-Z spectra"));

    /* For IV curves, there are always two spectra: forward and backward.
     * The backward one is really stored backwards, so we revert it upon
     * reading. */
    for (i = speccount = 0; speccount < nspectra; i++) {
        x = xscale*GINT16_FROM_LE(p16[pointstep*i]);
        y = yreal - yscale*GINT16_FROM_LE(p16[pointstep*i + 1]);
        n = (pointstep == 3) ? GINT16_FROM_LE(p16[pointstep*i + 2]) : 1;
        gwy_debug("IZ spec%d [%g,%g] %dpts", i, x, y, n);

        while (n--) {
            /* Only one direction */
            dline = make_iz_spectrum(res, xy_step,
                                     d16 + 2*speccount*res, 1e-12,
                                     FALSE);
            data = gwy_data_line_get_data(dline);
            gwy_spectra_add_spectrum(spectra, dline, x, y);
            g_object_unref(dline);

            speccount++;
        }
    }

    return spectra;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
