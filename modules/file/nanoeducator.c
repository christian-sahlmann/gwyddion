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
 *   <comment>Nanoedu SPM data header</comment>
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

static gboolean      module_register        (void);
static gint          nanoedu_detect         (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* nanoedu_load           (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gsize         nanoedu_read_header    (const guchar *buffer,
                                             gsize size,
                                             NanoeduFileHeader *header,
                                             GError **error);
static gsize         nanoedu_read_parameters(const guchar *buffer,
                                             gsize size,
                                             NanoeduParameterHeader *params,
                                             GError **error);
static GwyDataField* nanoedu_read_data_field(const guchar *buffer,
                                             gsize size,
                                             gint xres,
                                             gint yres,
                                             gdouble xreal,
                                             gdouble yreal,
                                             const gchar *xyunits,
                                             const char *zunits,
                                             gdouble q,
                                             GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanoeducator data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
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
    GwyDataField *dfield1 = NULL, *dfield2 = NULL;
    gdouble scale, q;
    const gchar *units, *title;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    p = buffer;

    if (!(len = nanoedu_read_header(p, size, &header, error)))
        goto fail;
    p += len;

    if (header.version < 11) {
        err_UNSUPPORTED(error, _("format version"));
        goto fail;
    }

    if (!(len = nanoedu_read_parameters(p, size - (p - buffer), &params,
                                        error)))
        goto fail;
    p += len;

    container = gwy_container_new();

    scale = Nanometer * params.xy_step;

    if (params.aqui_topo && header.topo_nx && header.topo_ny) {
        if (err_DIMENSION(error, header.topo_nx)
            || err_DIMENSION(error, header.topo_ny))
            goto fail;
        if (header.topo_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Image data starts past the end of file."));
            goto fail;
        }
        if (params.topo_nx != header.topo_nx)
            g_warning("params.topo_nx (%d) != header.topo_nx (%d), "
                      "choosing header", params.topo_nx, header.topo_nx);
        if (params.topo_ny != header.topo_ny)
            g_warning("params.topo_ny (%d) != header.topo_ny (%d), "
                      "choosing header", params.topo_ny, header.topo_ny);

        /* Version 12+ */
        q = 1e-3 * params.sens_z * params.amp_zgain * params.discr_z_mvolt;
        /* Version 11. */
        if (header.version == 11 || !q)
            q = 1.0;
        dfield1 = nanoedu_read_data_field(buffer + header.topo_offset,
                                          size - header.topo_offset,
                                          header.topo_nx,
                                          header.topo_ny,
                                          scale*header.topo_nx,
                                          scale*header.topo_ny,
                                          "m", "m", q*Nanometer, error);
        if (!dfield1) {
            gwy_object_unref(container);
            goto fail;
        }

        gwy_container_set_object_by_name(container, "/0/data", dfield1);
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("Topography"));
    }

    /* This seems to be the only way to recognize whether addsurf is present
     * because addsurf type 0 is a valid type. */
    if (header.addsurf_nx && header.addsurf_nx) {
        if (err_DIMENSION(error, header.addsurf_nx)
            || err_DIMENSION(error, header.addsurf_ny))
            goto fail;
        if (header.addsurf_offset >= size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Image data starts past the end of file."));
            goto fail;
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

        dfield2 = nanoedu_read_data_field(buffer + header.addsurf_offset,
                                          size - header.addsurf_offset,
                                          header.addsurf_nx,
                                          header.addsurf_ny,
                                          scale*header.addsurf_nx,
                                          scale*header.addsurf_ny,
                                          "m", units, q, error);
        if (!dfield1) {
            gwy_object_unref(container);
            goto fail;
        }

        gwy_container_set_object_by_name(container, "/1/data", dfield2);
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
    }

    if (!dfield1 && !dfield2) {
        err_NO_DATA(error);
        gwy_object_unref(container);
    }

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    gwy_object_unref(dfield1);
    gwy_object_unref(dfield2);

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

    params->sens_x = gwy_get_gfloat_le(&buffer);
    params->sens_y = gwy_get_gfloat_le(&buffer);
    params->sens_z = gwy_get_gfloat_le(&buffer);
    params->discr_z_mvolt = gwy_get_gfloat_le(&buffer);
    params->gain_x = gwy_get_gfloat_le(&buffer);
    params->gain_y = gwy_get_gfloat_le(&buffer);
    params->nA_D = gwy_get_gfloat_le(&buffer);
    params->V_D = gwy_get_gfloat_le(&buffer);
    gwy_debug("sens_z=%g, amp_zgain=%g, discr_z_mvolt=%g, V_D=%g, nA_D=%g, "
              "xy_step=%g",
              params->sens_z, params->amp_zgain, params->discr_z_mvolt,
              params->V_D, params->nA_D, params->xy_step);

    params->amp_modulation = gwy_get_gint16_le(&buffer); // XXX
    params->sd_gain_fm = gwy_get_guint16_le(&buffer);
    params->sd_gain_am = gwy_get_guint16_le(&buffer);
    params->res_freq_r = gwy_get_guint16_le(&buffer);
    params->res_freq_f = gwy_get_guint16_le(&buffer);
    params->f0 = gwy_get_gint16_le(&buffer); // XXX
    params->ampl_suppress = gwy_get_gfloat_le(&buffer);

    params->n_of_steps_x = gwy_get_gint16_le(&buffer);
    params->n_of_steps_y = gwy_get_gint16_le(&buffer);
    params->n_of_averaging = gwy_get_gint16_le(&buffer);
    params->spec_voltage_start = gwy_get_gfloat_le(&buffer);
    params->spec_voltage_final = gwy_get_gfloat_le(&buffer);
    params->time_spec_point = gwy_get_gfloat_le(&buffer);
    params->spec_modulation = gwy_get_gfloat_le(&buffer);
    params->spec_detector_coeff = gwy_get_gfloat_le(&buffer);
    params->resistance = gwy_get_gfloat_le(&buffer);
    params->reserved_spec1 = gwy_get_gint16_le(&buffer);
    params->reserved_spec2 = gwy_get_gint16_le(&buffer);
    params->reserved_spec3 = gwy_get_gfloat_le(&buffer);
    params->reserved_spec4 = gwy_get_gfloat_le(&buffer);

    params->cvc_type = gwy_get_gint16_le(&buffer);
    params->spectroscopy_type = gwy_get_gint16_le(&buffer);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
