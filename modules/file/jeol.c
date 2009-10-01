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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-jeol-spm">
 *   <comment>JEOL SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="II\x2a\x00\xd4\x17\x00\x00\x00\x04">
 *       <match type="string" offset="160" value="Measured by"/>
 *       <match type="string" offset="160" value="Mesuared by"/>
 *     </match>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * JEOL
 * .tif
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

/* Magic includes TIFF directory start address and size because they are fixed
 * numbers for JEOL enabling to quicky weed out most of other TIFF files. */
#define MAGIC      "II\x2a\x00\xd4\x17\x00\x00\x00\x04"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define Nanometer (1e-9)
#define Nanoampere (1e-9)
#define Nanovolt (1e-9)

enum {
    TIFF_HEADER_SIZE = 0x000a,
    JEOL_DICT_START  = 0x17d4,
    JEOL_DICT_SIZE   = 0x0400,
    JEOL_DATA_START  = JEOL_DICT_START + JEOL_DICT_SIZE
};

typedef enum {
    JEOL_IMAGE     = 1,
    JEOL_SPECTRUM  = 2,
    JEOL_PROFILE   = 3,
    JEOL_HISTOGRAM = 4,
    JEOL_VCO       = 5
} JEOLImageType;

typedef enum {
    JEOL_DATA_SOURCE_Z                 = 1,
    JEOL_DATA_SOURCE_LOG_I             = 2,
    JEOL_DATA_SOURCE_LIN_I             = 3,
    JEOL_DATA_SOURCE_AUX1              = 4,
    JEOL_DATA_SOURCE_AUX2              = 5,
    JEOL_DATA_SOURCE_AUX3              = 6,
    JEOL_DATA_SOURCE_EXT_VOLTAGE       = 7,
    JEOL_DATA_SOURCE_FORCE             = 8,
    JEOL_DATA_SOURCE_AFM               = 9,
    JEOL_DATA_SOURCE_FRICTION          = 10,
    JEOL_DATA_SOURCE_PHASE             = 11,
    JEOL_DATA_SOURCE_MFM               = 12,
    JEOL_DATA_SOURCE_ELASTICITY        = 13,
    JEOL_DATA_SOURCE_VISCOSITY         = 14,
    JEOL_DATA_SOURCE_FFM_FRICTION      = 15,
    JEOL_DATA_SOURCE_SURFACE_V         = 16,
    JEOL_DATA_SOURCE_PRESCAN           = 17,
    JEOL_DATA_SOURCE_RMS               = 18,
    JEOL_DATA_SOURCE_FMD               = 19,
    JEOL_DATA_SOURCE_CAPACITANCE_FORCE = 20
} JEOLDataSourceType;

typedef enum {
    JEOL_DISPLAY_DEFAULT = 1,
    JEOL_DISPLAY_BMP     = 2,
    JEOL_DISPLAY_3D      = 3
} JEOLDisplayModeType;

typedef enum {
    JEOL_MODE_LINE_1024    = 1,
    JEOL_MODE_TOPO_MIRROR  = 2,
    JEOL_MODE_TOPO_512     = 3,
    JEOL_MODE_TOPO_256     = 4,
    JEOL_MODE_TOPO_128     = 5,
    JEOL_MODE_LINE_512     = 6,
    JEOL_MODE_LINE_256     = 7,
    JEOL_MODE_LINE_128     = 8,
    JEOL_MODE_TOPO_X2      = 9,
    JEOL_MODE_TOPO_X4      = 10,
    JEOL_MODE_CITS         = 11,
    JEOL_MODE_I_V          = 12,
    JEOL_MODE_S_V          = 13,
    JEOL_MODE_I_S          = 14,
    JEOL_MODE_F_C          = 15,
    JEOL_MODE_FFC          = 16,
    JEOL_MODE_MONTAGE_128  = 17,
    JEOL_MODE_MONTAGE_256  = 18,
    JEOL_MODE_LSTS         = 19,
    JEOL_MODE_TOPO_SPS     = 20,
    JEOL_MODE_VCO          = 21,
    JEOL_MODE_TOPO_IMAGE   = 22,
    JEOL_MODE_TOPO3_VE_AFM = 23,
    JEOL_MODE_TOPO4_MFM    = 24,
    JEOL_MODE_TOPO3_LM_FFM = 25,
    JEOL_MODE_TOPO2_FKM    = 26,
    JEOL_MODE_TOPO2_FFM    = 27,
    JEOL_MODE_TOPO_1204    = 28,
    JEOL_MODE_TOPO_2X512   = 29,
    JEOL_MODE_TOPO2_SCFM   = 30,
    JEOL_MODE_TOPO2_MFM_1  = 31,
    JEOL_MODE_TOPO64       = 32,
    JEOL_MODE_PHASE_SHIFT  = 40,
    JEOL_MODE_MANIPULATION = 40,
    JEOL_MODE_CS3D_SCAN    = 50,
    JEOL_MODE_F_V          = 60,
    JEOL_MODE_SOFTWARE_GEN = 70
} JEOLModeType;

typedef enum {
    JEOL_AFM_MODE_CONTACT = 1 << 0,
    JEOL_AFM_MODE_SLOPE   = 1 << 1,
    JEOL_AFM_MODE_FM      = 1 << 2,
    JEOL_AFM_MODE_FMS     = 1 << 3,
    JEOL_AFM_MODE_PHASE   = 1 << 4
} JEOLAFMMode;

typedef enum {
    JEOL_SPM_MODE_NORMAL   = 1 << 0,
    JEOL_SPM_MODE_VE_AFM   = 1 << 1,
    JEOL_SPM_MODE_LM_AFM   = 1 << 2,
    JEOL_SPM_MODE_KFM      = 1 << 3,
    JEOL_SPM_MODE_MFM      = 1 << 4,
    JEOL_SPM_MODE_MFM_LINE = 1 << 5,
    JEOL_SPM_MODE_P_LIFT   = 1 << 6,
    JEOL_SPM_MODE_L_LIFT   = 1 << 7,
    JEOL_SPM_MODE_SCFM     = 1 << 8
} JEOLSPMMode;

typedef enum {
    JEOL_MEASUREMENT_SIGNAL_TOPOGRAPHY     = 1 << 0,
    JEOL_MEASUREMENT_SIGNAL_BIAS           = 1 << 1,
    JEOL_MEASUREMENT_SIGNAL_LINEAR_CURRENT = 1 << 2,
    JEOL_MEASUREMENT_SIGNAL_LOG_CURRENT    = 1 << 3,
    JEOL_MEASUREMENT_SIGNAL_FORCE          = 1 << 4,
    JEOL_MEASUREMENT_SIGNAL_FRICTION_FORCE = 1 << 5,
    JEOL_MEASUREMENT_SIGNAL_SUM            = 1 << 6,
    JEOL_MEASUREMENT_SIGNAL_RMS            = 1 << 7,
    JEOL_MEASUREMENT_SIGNAL_FMD            = 1 << 8,
    JEOL_MEASUREMENT_SIGNAL_PHASE          = 1 << 9,
    JEOL_MEASUREMENT_SIGNAL_AUX1           = 1 << 10,
    JEOL_MEASUREMENT_SIGNAL_AUX2           = 1 << 11,
    JEOL_MEASUREMENT_SIGNAL_AFM_CONTACT    = 1 << 12,
    JEOL_MEASUREMENT_SIGNAL_MOTOR_X_20     = 1 << 13,
    JEOL_MEASUREMENT_SIGNAL_MOTOR_Y_20     = 1 << 14,
    JEOL_MEASUREMENT_SIGNAL_MOTOR_Z_20     = 1 << 15,
    JEOL_MEASUREMENT_SIGNAL_AMB_APB        = 1 << 16,
    JEOL_MEASUREMENT_SIGNAL_AFM            = 1 << 17,
    JEOL_MEASUREMENT_SIGNAL_PRESCAN        = 1 << 18,
    JEOL_MEASUREMENT_SIGNAL_LATERAL_FORCE  = 1 << 19,
    JEOL_MEASUREMENT_SIGNAL_CMD_CPD        = 1 << 20,
    JEOL_MEASUREMENT_SIGNAL_NONE           = 1 << 21
} JEOLMeasurementSignal;

typedef struct {
    gint name;
    gint value;
} GwyFlatEnum;

typedef struct {
    guint day;
    guint month;
    guint year_1980;
    guint weekday_sun;
} JEOLDate;

typedef struct {
    guint hour;
    guint minute;
    guint second;
    guint second_100;
} JEOLTime;

typedef struct {
    guint x1;
    guint x2;
    guint y1;
    guint y2;
} JEOLSelectionRange;

typedef struct {
    guint adc_source;
    guint adc_offset;
    guint adc_gain;
    guint head_amp_gain;
} JEOLCITSParam;

typedef struct {
    gdouble clock;
    gdouble rotation;
    gdouble feedback_filter;
    gdouble present_filter;
    gdouble head_amp_gain;
    guint loop_gain;
    gdouble x_off;
    gdouble y_off;
    gdouble z_gain;
    gdouble z_off;
    gdouble o_gain;
    gdouble o_off;
    gdouble back_scan_bias;
    JEOLModeType mode;
} JEOLSPMParam;

typedef struct {
    gdouble dds_frequency;
    guint32 dds_low_filter;
    guint32 dds_high_filter;
    guint32 dds_center_filter;
    gboolean dds_enable;
    guint32 scan_filter;
    JEOLAFMMode afm_mode;
    guint32 slope_gain;
    guint32 x_addition_signal;
    guint32 y_addition_signal;
    guint32 z_addition_signal;
    guint32 bias_addition_signal;
    guint32 active_dialog;  /* actually an enum */
    JEOLSPMMode spm_mode;
    JEOLMeasurementSignal measurement_signal;
    guint32 phase_vco_scan;
    guint32 sps_mode;  /* actually an enum */
    gdouble dds_amplitude;
    gdouble dds_center_locked_freq;
    gdouble dds_phase_shift;
    guint32 dds_high_gain;
    guint32 dds_phase_polarity;
    guint32 dds_pll_excitation;
    guint32 dds_external;
    guint32 dds_rms_filter;
    guint32 dds_pll_loop_gain;
    guint32 dds_beat_noise;
    guint32 dds_dynamic_range;  /* actually an enum */
    gdouble cantilever_peak_freq;
    gdouble cantilever_q_factor;
} JEOLSPMParam1;

typedef struct {
    guint32 winspm_version;
    gchar internal_filename_old[80];
    guint32 xres;
    guint32 yres;
    gdouble xreal;
    gdouble yreal;
    gdouble z0;
    gdouble z255;
    gint32 adc_min;
    gint32 adc_max;
    gdouble initial_scan_scale;
    gchar internal_filename[40];
    gchar info[5][40];
    guchar history[50];
    gboolean has_current_info;
    gdouble bias;
    gdouble reference_value;
    JEOLDate measurement_date;
    JEOLDate save_date;
    JEOLTime measurement_time;
    JEOLTime save_time;
    guint32 fft_offset;
    guint32 transform_off;
    /* JEOLExtraType extra; union of JEOLSelectionRange and JEOLCITSParam */
    gboolean compressed;
    guint32 bpp;
    guint32 cits_offset;
    gdouble backscan_tip_voltage;
    guint32 sts_point_x;
    guint32 sts_point_y;
    gdouble tip_speed_x;
    gdouble tip_speed_y;
    /* JEOLCalib piezo_sensitivity; */
    JEOLSPMParam spm_param;
    guint32 montage_offset;
    gchar image_location[260];
    JEOLSPMParam1 spm_misc_param;
    guint32 sub_revision_no;
    JEOLImageType image_type;
    JEOLDataSourceType data_source;
    JEOLDisplayModeType display_mode;
    /* JEOLFileTime measurement_start_time; */
    /* JEOLFileTime measurement_end_time; */
    /* JEOLProfileDef profile_roughness; */
    /* JEOL3DSettings settings_3d; */
    gdouble lut_brightness;
    gdouble lut_contrast;
    guint32 software_version;
    guint32 software_subversion;
    /* JEOLExtract extracted_region; */
    gdouble lut_gamma;
    guint32 n_of_sps;
    guint32 sps_offset;
    guint32 line_trace_end_x;
    guint32 line_trace_end_y;
    gboolean forward;
    gboolean lift_signal;
    /* JEOLLIAParam lia_settings; */
    /* JEOLTempParam temp_param; */
    /* JEOLAFMFixedRef converted_afm; */
    /* JEOLLMCantileverParam special_measurement_param; */
} JEOLImageHeader;

static gboolean      module_register       (void);
static gint          jeol_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* jeol_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static void          jeol_read_image_header(const guchar *p,
                                            JEOLImageHeader *header);
static GwyDataField* jeol_read_data_field  (const guchar *buffer,
                                            const JEOLImageHeader *header);
static GwyContainer* jeol_get_metadata     (const JEOLImageHeader *header);

#ifdef GWY_RELOC_SOURCE
static const GwyEnum data_sources[] = {
    { "Topography",        JEOL_DATA_SOURCE_Z,                 },
    { "Log current",       JEOL_DATA_SOURCE_LOG_I,             },
    { "Current",           JEOL_DATA_SOURCE_LIN_I,             },
    { "Aux1",              JEOL_DATA_SOURCE_AUX1,              },
    { "Aux2",              JEOL_DATA_SOURCE_AUX2,              },
    { "Aux3",              JEOL_DATA_SOURCE_AUX3,              },
    { "Ext. voltage",      JEOL_DATA_SOURCE_EXT_VOLTAGE,       },
    { "Force",             JEOL_DATA_SOURCE_FORCE,             },
    { "AFM",               JEOL_DATA_SOURCE_AFM,               },
    { "Friction",          JEOL_DATA_SOURCE_FRICTION,          },
    { "Phase",             JEOL_DATA_SOURCE_PHASE,             },
    { "MFM",               JEOL_DATA_SOURCE_MFM,               },
    { "Elasticity",        JEOL_DATA_SOURCE_ELASTICITY,        },
    { "Viscosity",         JEOL_DATA_SOURCE_VISCOSITY,         },
    { "FFM friction",      JEOL_DATA_SOURCE_FFM_FRICTION,      },
    { "Surface V",         JEOL_DATA_SOURCE_SURFACE_V,         },
    { "Prescan",           JEOL_DATA_SOURCE_PRESCAN,           },
    { "RMS",               JEOL_DATA_SOURCE_RMS,               },
    { "FMD",               JEOL_DATA_SOURCE_FMD,               },
    { "Capacitance force", JEOL_DATA_SOURCE_CAPACITANCE_FORCE, },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit data_sources[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar data_sources_name[] =
    "Topography\000Log current\000Current\000Aux1\000Aux2\000Aux3\000Ext. vol"
    "tage\000Force\000AFM\000Friction\000Phase\000MFM\000Elasticity\000Viscos"
    "ity\000FFM friction\000Surface V\000Prescan\000RMS\000FMD\000Capacitance"
    " force";

static const GwyFlatEnum data_sources[] = {
    { 0, JEOL_DATA_SOURCE_Z },
    { 11, JEOL_DATA_SOURCE_LOG_I },
    { 23, JEOL_DATA_SOURCE_LIN_I },
    { 31, JEOL_DATA_SOURCE_AUX1 },
    { 36, JEOL_DATA_SOURCE_AUX2 },
    { 41, JEOL_DATA_SOURCE_AUX3 },
    { 46, JEOL_DATA_SOURCE_EXT_VOLTAGE },
    { 59, JEOL_DATA_SOURCE_FORCE },
    { 65, JEOL_DATA_SOURCE_AFM },
    { 69, JEOL_DATA_SOURCE_FRICTION },
    { 78, JEOL_DATA_SOURCE_PHASE },
    { 84, JEOL_DATA_SOURCE_MFM },
    { 88, JEOL_DATA_SOURCE_ELASTICITY },
    { 99, JEOL_DATA_SOURCE_VISCOSITY },
    { 109, JEOL_DATA_SOURCE_FFM_FRICTION },
    { 122, JEOL_DATA_SOURCE_SURFACE_V },
    { 132, JEOL_DATA_SOURCE_PRESCAN },
    { 140, JEOL_DATA_SOURCE_RMS },
    { 144, JEOL_DATA_SOURCE_FMD },
    { 148, JEOL_DATA_SOURCE_CAPACITANCE_FORCE },
};
#endif   /* }}} */

#ifdef GWY_RELOC_SOURCE
static const GwyEnum measurement_signals[] = {
    { "Topography",     JEOL_MEASUREMENT_SIGNAL_TOPOGRAPHY,     },
    { "Bias",           JEOL_MEASUREMENT_SIGNAL_BIAS,           },
    { "Linear Current", JEOL_MEASUREMENT_SIGNAL_LINEAR_CURRENT, },
    { "Log Current",    JEOL_MEASUREMENT_SIGNAL_LOG_CURRENT,    },
    { "Force",          JEOL_MEASUREMENT_SIGNAL_FORCE,          },
    { "Friction Force", JEOL_MEASUREMENT_SIGNAL_FRICTION_FORCE, },
    { "A+B (SUM)",      JEOL_MEASUREMENT_SIGNAL_SUM,            },
    { "RMS",            JEOL_MEASUREMENT_SIGNAL_RMS,            },
    { "FMD",            JEOL_MEASUREMENT_SIGNAL_FMD,            },
    { "Phase",          JEOL_MEASUREMENT_SIGNAL_PHASE,          },
    { "AUX1",           JEOL_MEASUREMENT_SIGNAL_AUX1,           },
    { "AUX2",           JEOL_MEASUREMENT_SIGNAL_AUX2,           },
    { "AFM Contact",    JEOL_MEASUREMENT_SIGNAL_AFM_CONTACT,    },
    { "Motor x/20",     JEOL_MEASUREMENT_SIGNAL_MOTOR_X_20,     },
    { "Motor y/20",     JEOL_MEASUREMENT_SIGNAL_MOTOR_Y_20,     },
    { "Motor z/20",     JEOL_MEASUREMENT_SIGNAL_MOTOR_Z_20,     },
    { "(A-B)/(A+B)",    JEOL_MEASUREMENT_SIGNAL_AMB_APB,        },
    { "AFM",            JEOL_MEASUREMENT_SIGNAL_AFM,            },
    { "Prescan",        JEOL_MEASUREMENT_SIGNAL_PRESCAN,        },
    { "Lateral Force",  JEOL_MEASUREMENT_SIGNAL_LATERAL_FORCE,  },
    { "(C-D)/(C+D)",    JEOL_MEASUREMENT_SIGNAL_CMD_CPD,        },
    { "None",           JEOL_MEASUREMENT_SIGNAL_NONE,           },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit measurement_signals[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar measurement_signals_name[] =
    "Topography\000Bias\000Linear Current\000Log Current\000Force\000Friction"
    " Force\000A+B (SUM)\000RMS\000FMD\000Phase\000AUX1\000AUX2\000AFM Contac"
    "t\000Motor x/20\000Motor y/20\000Motor z/20\000(A-B)/(A+B)\000AFM\000Pre"
    "scan\000Lateral Force\000(C-D)/(C+D)\000None";

static const GwyFlatEnum measurement_signals[] = {
    { 0, JEOL_MEASUREMENT_SIGNAL_TOPOGRAPHY },
    { 11, JEOL_MEASUREMENT_SIGNAL_BIAS },
    { 16, JEOL_MEASUREMENT_SIGNAL_LINEAR_CURRENT },
    { 31, JEOL_MEASUREMENT_SIGNAL_LOG_CURRENT },
    { 43, JEOL_MEASUREMENT_SIGNAL_FORCE },
    { 49, JEOL_MEASUREMENT_SIGNAL_FRICTION_FORCE },
    { 64, JEOL_MEASUREMENT_SIGNAL_SUM },
    { 74, JEOL_MEASUREMENT_SIGNAL_RMS },
    { 78, JEOL_MEASUREMENT_SIGNAL_FMD },
    { 82, JEOL_MEASUREMENT_SIGNAL_PHASE },
    { 88, JEOL_MEASUREMENT_SIGNAL_AUX1 },
    { 93, JEOL_MEASUREMENT_SIGNAL_AUX2 },
    { 98, JEOL_MEASUREMENT_SIGNAL_AFM_CONTACT },
    { 110, JEOL_MEASUREMENT_SIGNAL_MOTOR_X_20 },
    { 121, JEOL_MEASUREMENT_SIGNAL_MOTOR_Y_20 },
    { 132, JEOL_MEASUREMENT_SIGNAL_MOTOR_Z_20 },
    { 143, JEOL_MEASUREMENT_SIGNAL_AMB_APB },
    { 155, JEOL_MEASUREMENT_SIGNAL_AFM },
    { 159, JEOL_MEASUREMENT_SIGNAL_PRESCAN },
    { 167, JEOL_MEASUREMENT_SIGNAL_LATERAL_FORCE },
    { 181, JEOL_MEASUREMENT_SIGNAL_CMD_CPD },
    { 193, JEOL_MEASUREMENT_SIGNAL_NONE },
};
#endif   /* }}} */

#ifdef GWY_RELOC_SOURCE
static const GwyEnum afm_modes[] = {
    { "Contact", JEOL_AFM_MODE_CONTACT, },
    { "Slope",   JEOL_AFM_MODE_SLOPE,   },
    { "FM",      JEOL_AFM_MODE_FM,      },
    { "FMS",     JEOL_AFM_MODE_FMS,     },
    { "Phase",   JEOL_AFM_MODE_PHASE,   },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit afm_modes[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar afm_modes_name[] =
    "Contact\000Slope\000FM\000FMS\000Phase";

static const GwyFlatEnum afm_modes[] = {
    { 0, JEOL_AFM_MODE_CONTACT },
    { 8, JEOL_AFM_MODE_SLOPE },
    { 14, JEOL_AFM_MODE_FM },
    { 17, JEOL_AFM_MODE_FMS },
    { 21, JEOL_AFM_MODE_PHASE },
};
#endif   /* }}} */

#ifdef GWY_RELOC_SOURCE
static const GwyEnum spm_modes[] = {
    { "Normal",   JEOL_SPM_MODE_NORMAL,   },
    { "VE-AFM",   JEOL_SPM_MODE_VE_AFM,   },
    { "LM-AFM",   JEOL_SPM_MODE_LM_AFM,   },
    { "KFM",      JEOL_SPM_MODE_KFM,      },
    { "MFM",      JEOL_SPM_MODE_MFM,      },
    { "MFM-line", JEOL_SPM_MODE_MFM_LINE, },
    { "P-lift",   JEOL_SPM_MODE_P_LIFT,   },
    { "L-lift",   JEOL_SPM_MODE_L_LIFT,   },
    { "SCFM",     JEOL_SPM_MODE_SCFM,     },
};
#else  /* {{{ */
/* This code block was GENERATED by flatten.py.
   When you edit spm_modes[] data above,
   re-run flatten.py SOURCE.c. */
static const gchar spm_modes_name[] =
    "Normal\000VE-AFM\000LM-AFM\000KFM\000MFM\000MFM-line\000P-lift\000L-lift"
    "\000SCFM";

static const GwyFlatEnum spm_modes[] = {
    { 0, JEOL_SPM_MODE_NORMAL },
    { 7, JEOL_SPM_MODE_VE_AFM },
    { 14, JEOL_SPM_MODE_LM_AFM },
    { 21, JEOL_SPM_MODE_KFM },
    { 25, JEOL_SPM_MODE_MFM },
    { 29, JEOL_SPM_MODE_MFM_LINE },
    { 38, JEOL_SPM_MODE_P_LIFT },
    { 45, JEOL_SPM_MODE_L_LIFT },
    { 52, JEOL_SPM_MODE_SCFM },
};
#endif   /* }}} */

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports JEOL data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("jeol",
                           N_("JEOL data files (.tif)"),
                           (GwyFileDetectFunc)&jeol_detect,
                           (GwyFileLoadFunc)&jeol_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
jeol_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    if (only_name)
        return 0;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0
        || fileinfo->file_size < JEOL_DATA_START)
        return 0;

    /* TODO: check it better, some TIFFs can have the same dictionary start
     * and size by chance. */
    return 90;
}

/***** Generic **************************************************************/
static const gchar*
gwy_flat_enum_to_string(gint enumval,
                        guint nentries,
                        const GwyFlatEnum *table,
                        const gchar *names)
{
    gint j;

    for (j = 0; j < nentries; j++) {
        if (enumval == table[j].value)
            return names + table[j].name;
    }

    return NULL;
}

static gchar*
gwy_flat_flags_to_string(gint enumval,
                         guint nentries,
                         const GwyFlatEnum *table,
                         const gchar *names,
                         const gchar *glue)
{
    GString *str;
    gchar *retval;
    gint j;

    str = g_string_new(NULL);
    for (j = 0; j < nentries; j++) {
        if (enumval & table[j].value) {
            if (str->len)
                g_string_append(str, glue ? glue : " ");
            g_string_append(str, names + table[j].name);
        }
    }
    retval = str->str;
    g_string_free(str, FALSE);

    return retval;
}
/****************************************************************************/

static GwyContainer*
jeol_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    JEOLImageHeader image_header;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gsize expected_size, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const gchar *title;
    gchar *s;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < JEOL_DATA_START) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "JEOL");
        goto fail;
    }

    jeol_read_image_header(buffer, &image_header);
    /* Elementrary sanity */
    if (image_header.bpp != 16) {
        err_BPP(error, image_header.bpp);
        goto fail;
    }

    if (err_DIMENSION(error, image_header.xres)
        || err_DIMENSION(error, image_header.yres))
        goto fail;

    expected_size = image_header.bpp/8 * image_header.xres*image_header.yres;
    if (err_SIZE_MISMATCH(error, JEOL_DATA_START + expected_size, size, FALSE))
        goto fail;

    if (image_header.image_type != JEOL_IMAGE || image_header.compressed) {
        err_NO_DATA(error);
        goto fail;
    }

    /* Use negated positive conditions to catch NaNs */
    if (!((image_header.xreal = fabs(image_header.xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        image_header.xreal = 1.0;
    }
    if (!((image_header.yreal = fabs(image_header.yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        image_header.yreal = 1.0;
    }

    dfield = jeol_read_data_field(buffer + JEOL_DATA_START, &image_header);
    if (!dfield) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("The type of data is unknown.  "
                      "Please report it to the developers."));
        goto fail;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    /* Title */
    s = g_convert(image_header.internal_filename, -1,
                  "iso-8859-1", "utf-8", NULL, NULL, NULL);
    if (s)
        g_strstrip(s);
    if (s && *s)
        gwy_container_set_string_by_name(container, "/0/data/title", s);
    else {
        title = gwy_flat_enum_to_string(image_header.data_source,
                                        G_N_ELEMENTS(data_sources),
                                        data_sources, data_sources_name);
        if (title)
            gwy_container_set_string_by_name(container, "/0/data/title",
                                             g_strdup(title));
    }

    /* Meta */
    meta = jeol_get_metadata(&image_header);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

static void
jeol_read_image_header(const guchar *p,
                       JEOLImageHeader *header)
{
    p += TIFF_HEADER_SIZE;

    header->winspm_version = gwy_get_guint16_le(&p);
    p += 80;  /* there isn't anything interesting in internal_filename_old */
    header->xres = gwy_get_guint16_le(&p);
    header->yres = gwy_get_guint16_le(&p);
    header->xreal = gwy_get_gfloat_le(&p);
    header->yreal = gwy_get_gfloat_le(&p);
    gwy_debug("res: (%d,%d), real: (%g,%g)",
              header->xres, header->yres, header->xreal, header->yreal);
    header->z0 = gwy_get_gfloat_le(&p);
    header->z255 = gwy_get_gfloat_le(&p);
    header->adc_min = gwy_get_guint16_le(&p);
    header->adc_max = gwy_get_guint16_le(&p);
    header->initial_scan_scale = gwy_get_gfloat_le(&p);
    get_CHARARRAY0(header->internal_filename, &p);
    get_CHARARRAY0(header->info[0], &p);
    get_CHARARRAY0(header->info[1], &p);
    get_CHARARRAY0(header->info[2], &p);
    get_CHARARRAY0(header->info[3], &p);
    get_CHARARRAY0(header->info[4], &p);
    get_CHARARRAY(header->history, &p);
    header->has_current_info = gwy_get_guint16_le(&p);
    header->bias = gwy_get_gfloat_le(&p);
    gwy_debug("bias: %g", header->bias);
    header->reference_value = gwy_get_gfloat_le(&p);
    gwy_debug("reference_value: %g", header->reference_value);
    p += 2;  /* reserved */
    header->measurement_date.day = *(p++);
    header->measurement_date.month = *(p++);
    header->measurement_date.year_1980 = gwy_get_guint16_le(&p);
    header->measurement_date.weekday_sun = *(p++);
    header->save_date.day = *(p++);
    header->save_date.month = *(p++);
    header->save_date.year_1980 = gwy_get_guint16_le(&p);
    header->save_date.weekday_sun = *(p++);
    header->measurement_time.hour = *(p++);
    header->measurement_time.minute = *(p++);
    header->measurement_time.second= *(p++);
    header->measurement_time.second_100 = *(p++);
    header->save_time.hour = *(p++);
    header->save_time.minute = *(p++);
    header->save_time.second= *(p++);
    header->save_time.second_100 = *(p++);
    p += 0x100;  /* lookup table */
    header->fft_offset = gwy_get_guint32_le(&p);
    header->transform_off = gwy_get_guint16_le(&p);
    p += 8;  /* extra */
    header->compressed = gwy_get_gboolean8(&p);
    header->bpp = *(p++);
    gwy_debug("bpp: %d", header->bpp);
    header->cits_offset = gwy_get_guint32_le(&p);
    header->backscan_tip_voltage = gwy_get_gfloat_le(&p);
    header->sts_point_x = gwy_get_guint16_le(&p);
    header->sts_point_y = gwy_get_guint16_le(&p);
    p += 20;  /* reserved */
    header->tip_speed_x = gwy_get_gfloat_le(&p);
    header->tip_speed_y = gwy_get_gfloat_le(&p);
    p += 42;  /* piezo_sensitivity */
    header->spm_param.clock = gwy_get_gfloat_le(&p);
    header->spm_param.rotation = gwy_get_gfloat_le(&p);
    header->spm_param.feedback_filter = gwy_get_gfloat_le(&p);
    header->spm_param.present_filter = gwy_get_gfloat_le(&p);
    header->spm_param.head_amp_gain = gwy_get_gfloat_le(&p);
    header->spm_param.loop_gain = gwy_get_guint16_le(&p);
    header->spm_param.x_off = gwy_get_gfloat_le(&p);
    header->spm_param.y_off = gwy_get_gfloat_le(&p);
    header->spm_param.z_gain = gwy_get_gfloat_le(&p);
    header->spm_param.z_off = gwy_get_gfloat_le(&p);
    header->spm_param.o_gain = gwy_get_gfloat_le(&p);
    header->spm_param.o_off = gwy_get_gfloat_le(&p);
    gwy_debug("z_gain: %g o_gain: %g",
              header->spm_param.z_gain, header->spm_param.o_gain);
    header->spm_param.back_scan_bias = gwy_get_gfloat_le(&p);
    /* XXX: This does not match what the documentation says, there seems to be
     * a four-byte quantity missing between back_scan_bias and mode (the size
     * of SPM params struct is also 4 bytes shorter than the space alloted for
     * it). */
    p += 4;  /* whatever */
    header->spm_param.mode = gwy_get_guint32_le(&p);
    gwy_debug("mode: %d", header->spm_param.mode);
    p += 2;  /* reserved */
    header->montage_offset = gwy_get_guint32_le(&p);
    get_CHARARRAY0(header->image_location, &p);
    p += 2*4;  /* reserved */
    header->spm_misc_param.dds_frequency = gwy_get_gfloat_le(&p);
    header->spm_misc_param.dds_low_filter = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_high_filter = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_center_filter = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_enable = gwy_get_guint16_le(&p);
    header->spm_misc_param.scan_filter = gwy_get_guint16_le(&p);
    header->spm_misc_param.afm_mode = gwy_get_guint32_le(&p);
    gwy_debug("afm_mode: 0x%x", header->spm_misc_param.afm_mode);
    header->spm_misc_param.slope_gain = gwy_get_guint32_le(&p);
    header->spm_misc_param.x_addition_signal = gwy_get_guint16_le(&p);
    header->spm_misc_param.y_addition_signal = gwy_get_guint16_le(&p);
    header->spm_misc_param.z_addition_signal = gwy_get_guint16_le(&p);
    header->spm_misc_param.bias_addition_signal = gwy_get_guint16_le(&p);
    header->spm_misc_param.active_dialog = gwy_get_guint32_le(&p);
    header->spm_misc_param.spm_mode = gwy_get_guint32_le(&p);
    gwy_debug("spm_mode: 0x%x", header->spm_misc_param.spm_mode);
    header->spm_misc_param.measurement_signal = gwy_get_guint32_le(&p);
    gwy_debug("signal: 0x%x", header->spm_misc_param.measurement_signal);
    header->spm_misc_param.phase_vco_scan = gwy_get_guint16_le(&p);
    header->spm_misc_param.sps_mode = gwy_get_guint32_le(&p);
    header->spm_misc_param.dds_amplitude = gwy_get_gdouble_le(&p);
    header->spm_misc_param.dds_center_locked_freq = gwy_get_gdouble_le(&p);
    header->spm_misc_param.dds_phase_shift = gwy_get_gfloat_le(&p);
    header->spm_misc_param.dds_high_gain = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_phase_polarity = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_pll_excitation = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_external = gwy_get_guint16_le(&p);
    header->spm_misc_param.dds_rms_filter = gwy_get_guint32_le(&p);
    header->spm_misc_param.dds_pll_loop_gain = gwy_get_guint32_le(&p);
    header->spm_misc_param.dds_beat_noise = gwy_get_guint32_le(&p);
    header->spm_misc_param.dds_dynamic_range = gwy_get_guint32_le(&p);
    header->spm_misc_param.cantilever_peak_freq = gwy_get_guint32_le(&p);
    header->spm_misc_param.cantilever_q_factor = gwy_get_guint32_le(&p);
    p += 10;  /* reserved */
    p += 0x300;  /* RGB lookup table */
    header->sub_revision_no = gwy_get_guint32_le(&p);
    header->image_type = gwy_get_guint32_le(&p);
    gwy_debug("image_type: %d", header->image_type);
    header->data_source = gwy_get_guint32_le(&p);
    gwy_debug("data_source: %d", header->data_source);
    header->display_mode = gwy_get_guint32_le(&p);
    p += 2*8;  /* measurement_start_time, measurement_end_time */
    p += 254;  /* profile_roughness */
    p += 446;  /* settings_3d */
    header->lut_brightness = gwy_get_gfloat_le(&p);
    header->lut_contrast = gwy_get_gfloat_le(&p);
    header->software_version = gwy_get_guint16_le(&p);
    header->software_subversion = gwy_get_guint32_le(&p);
    p += 20;  /* extracted_region */
    header->lut_gamma = gwy_get_gfloat_le(&p);
    header->n_of_sps = gwy_get_guint16_le(&p);
    header->sps_offset = gwy_get_guint32_le(&p);
    header->line_trace_end_x = gwy_get_guint16_le(&p);
    header->line_trace_end_y = gwy_get_guint16_le(&p);
    header->forward = gwy_get_guint16_le(&p);
    gwy_debug("forward: %d", header->forward);
    header->lift_signal = gwy_get_guint16_le(&p);
    /* stuff... */
}

static GwyDataField*
jeol_read_data_field(const guchar *buffer,
                     const JEOLImageHeader *header)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const guint16 *d16;
    gdouble *data;
    gdouble q, z0;
    gint i;

    switch (header->spm_misc_param.measurement_signal) {
        case JEOL_MEASUREMENT_SIGNAL_TOPOGRAPHY:
        z0 = Nanometer*header->z0;
        q = (header->z255 - header->z0)/65535.0*Nanometer;
        siunit = gwy_si_unit_new("m");
        break;

        case JEOL_MEASUREMENT_SIGNAL_LINEAR_CURRENT:
        z0 = Nanoampere*header->z0;
        q = (header->z255 - header->z0)/65535.0*Nanoampere;
        siunit = gwy_si_unit_new("A");
        break;

        /* We just guess it's always voltage.  At least sometimes it is. */
        case JEOL_MEASUREMENT_SIGNAL_AUX1:
        case JEOL_MEASUREMENT_SIGNAL_AUX2:
        z0 = header->z0;
        q = (header->z255 - header->z0)/65535.0;
        siunit = gwy_si_unit_new("V");
        break;

        default:
        return NULL;
        break;
    }

    dfield = gwy_data_field_new(header->xres, header->yres,
                                Nanometer*header->xreal,
                                Nanometer*header->yreal,
                                FALSE);

    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    data = gwy_data_field_get_data(dfield);
    d16 = (const guint16*)buffer;
    for (i = 0; i < header->xres*header->yres; i++)
        data[i] = q*GUINT16_FROM_LE(d16[i]) + z0;

    return dfield;
}

static void
format_meta(GwyContainer *meta,
            const gchar *name,
            const gchar *format,
            ...)
{
    gchar *s;
    va_list ap;

    va_start(ap, format);
    s = g_strdup_vprintf(format, ap);
    va_end(ap);
    gwy_container_set_string_by_name(meta, name, s);
}

static void
format_bit(GwyContainer *meta,
           const gchar *name,
           guint n,
           const GwyFlatEnum *table,
           const gchar *names,
           guint value)
{
    gchar *t;

    t = gwy_flat_flags_to_string(value, n, table, names, NULL);
    if (t)
        gwy_container_set_string_by_name(meta, name, t);
    else
        g_free(t);
}

static GwyContainer*
jeol_get_metadata(const JEOLImageHeader *header)
{
    const JEOLSPMParam *spm_param;
    const JEOLDate *date;
    const JEOLTime *time_;
    GwyContainer *meta;
    const gchar *s;

    meta = gwy_container_new();

    spm_param = &header->spm_param;
    format_meta(meta, "Clock", "%g ms", spm_param->clock);
    format_meta(meta, "Rotation", "%g deg", spm_param->rotation);
    format_meta(meta, "Feedback filter", "%g Hz", spm_param->feedback_filter);
    format_meta(meta, "Present filter", "%g Hz", spm_param->present_filter);
    format_meta(meta, "Head amp gain", "%g V/nA", spm_param->head_amp_gain);
    if ((s = gwy_enuml_to_string(spm_param->mode,
                                 "Line1024", JEOL_MODE_LINE_1024,
                                 "Topo Mirror", JEOL_MODE_TOPO_MIRROR,
                                 "Topo512", JEOL_MODE_TOPO_512,
                                 "Topo256", JEOL_MODE_TOPO_256,
                                 "Topo128", JEOL_MODE_TOPO_128,
                                 "Line512", JEOL_MODE_LINE_512,
                                 "Line256", JEOL_MODE_LINE_256,
                                 "Line128", JEOL_MODE_LINE_128,
                                 "Topo ×2", JEOL_MODE_TOPO_X2,
                                 "Topo ×4", JEOL_MODE_TOPO_X4,
                                 "CITS", JEOL_MODE_CITS,
                                 "I-V", JEOL_MODE_I_V,
                                 "S-V", JEOL_MODE_S_V,
                                 "I-S", JEOL_MODE_I_S,
                                 "F-C", JEOL_MODE_F_C,
                                 "FFC", JEOL_MODE_FFC,
                                 "Montage128", JEOL_MODE_MONTAGE_128,
                                 "Montage256", JEOL_MODE_MONTAGE_256,
                                 "LSTS", JEOL_MODE_LSTS,
                                 "Topo SPS", JEOL_MODE_TOPO_SPS,
                                 "VCO", JEOL_MODE_VCO,
                                 "Topo Image", JEOL_MODE_TOPO_IMAGE,
                                 "Topo3 VE AFM", JEOL_MODE_TOPO3_VE_AFM,
                                 "Topo4 MFM", JEOL_MODE_TOPO4_MFM,
                                 "Topo3 LM FFM", JEOL_MODE_TOPO3_LM_FFM,
                                 "Topo2 FKM", JEOL_MODE_TOPO2_FKM,
                                 "Topo2 FFM", JEOL_MODE_TOPO2_FFM,
                                 "Topo1204", JEOL_MODE_TOPO_1204,
                                 "Topo 2×512", JEOL_MODE_TOPO_2X512,
                                 "Topo2 SCFM", JEOL_MODE_TOPO2_SCFM,
                                 "Topo2 MFM-1", JEOL_MODE_TOPO2_MFM_1,
                                 "Topo64", JEOL_MODE_TOPO64,
                                 "Phaseshift", JEOL_MODE_PHASE_SHIFT,
                                 "Manipulation", JEOL_MODE_MANIPULATION,
                                 "CS3D Scan", JEOL_MODE_CS3D_SCAN,
                                 "F-V", JEOL_MODE_F_V,
                                 "SoftwareGen", JEOL_MODE_SOFTWARE_GEN,
                                 NULL)))
        format_meta(meta, "Measurement mode", "%s", s);

    format_meta(meta, "Bias", "%g V", header->bias);
    /* FIXME: It's called `reference value' and it can be in V or nA.
     * What it means when it's V and how one can tell when it's what? */
    format_meta(meta, "Tunnel current", "%g nA", header->reference_value);

    date = &header->measurement_date;
    time_ = &header->measurement_time;
    format_meta(meta, "Date and time of measurement",
                "%04d-%02d-%02d %02d:%02d:%02d.%02d",
                date->year_1980, date->month, date->day,
                time_->hour, time_->minute, time_->second, time_->second_100);

    date = &header->save_date;
    time_ = &header->save_time;
    format_meta(meta, "Date and time of file save",
                "%04d-%02d-%02d %02d:%02d:%02d.%02d",
                date->year_1980, date->month, date->day,
                time_->hour, time_->minute, time_->second, time_->second_100);

    format_meta(meta, "Tip speed X", "%g nm/s", header->tip_speed_x);
    format_meta(meta, "Tip speed Y", "%g nm/s", header->tip_speed_y);
    if ((s = gwy_flat_enum_to_string(header->data_source,
                                     G_N_ELEMENTS(data_sources),
                                     data_sources, data_sources_name)))
        format_meta(meta, "Data source", "%s", s);
    format_meta(meta, "Direction", header->forward ? "Forward" : "Backward");

    format_bit(meta, "Measurement signal",
               G_N_ELEMENTS(measurement_signals), measurement_signals,
               measurement_signals_name,
               header->spm_misc_param.measurement_signal);
    format_bit(meta, "SPM mode",
               G_N_ELEMENTS(spm_modes), spm_modes, spm_modes_name,
               header->spm_misc_param.spm_mode);
    format_bit(meta, "AFM mode",
               G_N_ELEMENTS(afm_modes), afm_modes, afm_modes_name,
               header->spm_misc_param.afm_mode);

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
