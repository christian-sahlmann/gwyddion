/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
/* TODO: some metadata, MDA, ... */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "\x01\xb0\x93\xff"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".mdt"

#define Angstrom (1e-10)
#define Nano (1e-9)

typedef enum {
    MDT_FRAME_SCANNED      = 0,
    MDT_FRAME_SPECTROSCOPY = 1,
    MDT_FRAME_TEXT         = 3,
    MDT_FRAME_OLD_MDA      = 105,
    MDT_FRAME_MDA          = 106,
    MDT_FRAME_PALETTE      = 107
} MDTFrameType;

typedef enum {
    MDT_UNIT_RAMAN_SHIFT     = -10,
    MDT_UNIT_RESERVED0       = -9,
    MDT_UNIT_RESERVED1       = -8,
    MDT_UNIT_RESERVED2       = -7,
    MDT_UNIT_RESERVED3       = -6,
    MDT_UNIT_METER           = -5,
    MDT_UNIT_CENTIMETER      = -4,
    MDT_UNIT_MILLIMETER      = -3,
    MDT_UNIT_MIKROMETER      = -2,
    MDT_UNIT_NANOMETER       = -1,
    MDT_UNIT_ANGSTROM        = 0,
    MDT_UNIT_NANOAMPERE      = 1,
    MDT_UNIT_VOLT            = 2,
    MDT_UNIT_NONE            = 3,
    MDT_UNIT_KILOHERZ        = 4,
    MDT_UNIT_DEGREES         = 5,
    MDT_UNIT_PERCENT         = 6,
    MDT_UNIT_CELSIUM_DEGREE  = 7,
    MDT_UNIT_VOLT_HIGH       = 8,
    MDT_UNIT_SECOND          = 9,
    MDT_UNIT_MILLISECOND     = 10,
    MDT_UNIT_MIKROSECOND     = 11,
    MDT_UNIT_NANOSECOND      = 12,
    MDT_UNIT_COUNTS          = 13,
    MDT_UNIT_PIXELS          = 14,
    MDT_UNIT_RESERVED_SFOM0  = 15,
    MDT_UNIT_RESERVED_SFOM1  = 16,
    MDT_UNIT_RESERVED_SFOM2  = 17,
    MDT_UNIT_RESERVED_SFOM3  = 18,
    MDT_UNIT_RESERVED_SFOM4  = 19,
    MDT_UNIT_AMPERE2         = 20,
    MDT_UNIT_MILLIAMPERE     = 21,
    MDT_UNIT_MIKROAMPERE     = 22,
    MDT_UNIT_NANOAMPERE2     = 23,
    MDT_UNIT_PICOAMPERE      = 24,
    MDT_UNIT_VOLT2           = 25,
    MDT_UNIT_MILLIVOLT       = 26,
    MDT_UNIT_MIKROVOLT       = 27,
    MDT_UNIT_NANOVOLT        = 28,
    MDT_UNIT_PICOVOLT        = 29,
    MDT_UNIT_NEWTON          = 30,
    MDT_UNIT_MILLINEWTON     = 31,
    MDT_UNIT_MIKRONEWTON     = 32,
    MDT_UNIT_NANONEWTON      = 33,
    MDT_UNIT_PICONEWTON      = 34,
    MDT_UNIT_RESERVED_DOS0   = 35,
    MDT_UNIT_RESERVED_DOS1   = 36,
    MDT_UNIT_RESERVED_DOS2   = 37,
    MDT_UNIT_RESERVED_DOS3   = 38,
    MDT_UNIT_RESERVED_DOS4   = 39
} MDTUnit;

typedef enum {
    MDT_MODE_STM = 0,
    MDT_MODE_AFM = 1
} MDTMode;

typedef enum {
    MDT_INPUT_EXTENSION_SLOT = 0,
    MDT_INPUT_BIAS_V         = 1,
    MDT_INPUT_GROUND         = 2
} MDTInputSignal;

typedef enum {
    MDT_TUNE_STEP  = 0,
    MDT_TUNE_FINE  = 1,
    MDT_TUNE_SLOPE = 2
} MDTLiftMode;

typedef enum {
    MDT_SPM_TECHNIQUE_CONTACT_MODE     = 0,
    MDT_SPM_TECHNIQUE_SEMICONTACT_MODE = 1,
    MDT_SPM_TECHNIQUE_TUNNEL_CURRENT   = 2,
    MDT_SPM_TECHNIQUE_SNOM             = 3
} MDTSPMTechnique;

typedef enum {
    MDT_SPM_MODE_CONSTANT_FORCE               = 0,
    MDT_SPM_MODE_CONTACT_CONSTANT_HEIGHT      = 1,
    MDT_SPM_MODE_CONTACT_ERROR                = 2,
    MDT_SPM_MODE_LATERAL_FORCE                = 3,
    MDT_SPM_MODE_FORCE_MODULATION             = 4,
    MDT_SPM_MODE_SPREADING_RESISTANCE_IMAGING = 5,
    MDT_SPM_MODE_SEMICONTACT_TOPOGRAPHY       = 6,
    MDT_SPM_MODE_SEMICONTACT_ERROR            = 7,
    MDT_SPM_MODE_PHASE_CONTRAST               = 8,
    MDT_SPM_MODE_AC_MAGNETIC_FORCE            = 9,
    MDT_SPM_MODE_DC_MAGNETIC_FORCE            = 10,
    MDT_SPM_MODE_ELECTROSTATIC_FORCE          = 11,
    MDT_SPM_MODE_CAPACITANCE_CONTRAST         = 12,
    MDT_SPM_MODE_KELVIN_PROBE                 = 13,
    MDT_SPM_MODE_CONSTANT_CURRENT             = 14,
    MDT_SPM_MODE_BARRIER_HEIGHT               = 15,
    MDT_SPM_MODE_CONSTANT_HEIGHT              = 16,
    MDT_SPM_MODE_AFAM                         = 17,
    MDT_SPM_MODE_CONTACT_EFM                  = 18,
    MDT_SPM_MODE_SHEAR_FORCE_TOPOGRAPHY       = 19,
    MDT_SPM_MODE_SFOM                         = 20,
    MDT_SPM_MODE_CONTACT_CAPACITANCE          = 21,
    MDT_SPM_MODE_SNOM_TRANSMISSION            = 22,
    MDT_SPM_MODE_SNOM_REFLECTION              = 23,
    MDT_SPM_MODE_SNOM_ALL                     = 24,
    MDT_SPM_MODE_SNOM                         = 25
} MDTSPMMode;

enum {
    FILE_HEADER_SIZE      = 32,
    FRAME_HEADER_SIZE     = 22,
    FRAME_MODE_SIZE       = 8,
    AXIS_SCALES_SIZE      = 30,
    SCAN_VARS_MIN_SIZE    = 77,
    SPECTRO_VARS_MIN_SIZE = 38
};

typedef struct {
    gdouble offset;    /* r0 (physical units) */
    gdouble step;    /* r (physical units) */
    MDTUnit unit;    /* U */
} MDTAxisScale;

typedef struct {
    MDTAxisScale x_scale;
    MDTAxisScale y_scale;
    MDTAxisScale z_scale;
    gint channel_index;    /* s_mode */
    MDTMode mode;    /* s_dev */
    gint xres;    /* s_nx */
    gint yres;    /* s_ny */
    gint ndacq;    /* s_rv6; obsolete */
    gdouble step_length;    /* s_rs */
    guint adt;    /* s_adt */
    guint adc_gain_amp_log10;    /* s_adc_a */
    guint adc_index;    /* s_a12 */
    /* XXX: Some fields have different meaning in different versions */
    union {
        guint input_signal;    /* MDTInputSignal smp_in; s_smp_in */
        guint version;    /* s_8xx */
    } s16;
    union {
        guint substr_plane_order;    /* s_spl */
        guint pass_num;    /* z_03 */
    } s17;
    guint scan_dir;    /* s_xy TODO: interpretation */
    gboolean power_of_2;    /* s_2n */
    gdouble velocity;    /* s_vel (Angstrom/second) */
    gdouble setpoint;    /* s_i0 */
    gdouble bias_voltage;    /* s_ut */
    gboolean draw;    /* s_draw */
    gint xoff;    /* s_x00 (in DAC quants) */
    gint yoff;    /* s_y00 (in DAC quants) */
    gboolean nl_corr;    /* s_cor */
#if 0
    guint orig_format;    /* s_oem */
    MDTLiftMode tune;    /* z_tune */
    gdouble feedback_gain;    /* s_fbg */
    gint dac_scale;    /* s_s */
    gint overscan;    /* s_xov (in %) */
#endif
    /* XXX: much more stuff here */

    /* Frame mode stuff */
    guint fm_mode;    /* m_mode */
    guint fm_xres;    /* m_nx */
    guint fm_yres;    /* m_ny */
    guint fm_ndots;    /* m_nd */

    /* Data */
    const guchar *dots;
    const guchar *image;

    /* Stuff after data */
    guint title_len;
    const guchar *title;
    gchar *xmlstuff;
} MDTScannedDataFrame;

typedef struct {
    guint size;     /* h_sz */
    MDTFrameType type;     /* h_what */
    gint version;  /* h_ver0, h_ver1 */

    gint year;    /* h_yea */
    gint month;    /* h_mon */
    gint day;    /* h_day */
    gint hour;    /* h_h */
    gint min;    /* h_m */
    gint sec;    /* h_s */

    gint var_size;    /* h_am, v6 and older only */

    gpointer frame_data;
} MDTFrame;

typedef struct {
    guint size;  /* f_sz */
    guint last_frame; /* f_nt */
    MDTFrame *frames;
} MDTFile;

static gboolean      module_register       (void);
static gint          mdt_detect            (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* mdt_load              (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyContainer* mdt_get_metadata      (MDTFile *mdtfile,
                                            guint i);
static void          mdt_add_frame_metadata(MDTScannedDataFrame *sdframe,
                                            GwyContainer *meta);
static gboolean      mdt_real_load         (const guchar *buffer,
                                            guint size,
                                            MDTFile *mdtfile,
                                            GError **error);
static GwyDataField* extract_scanned_data  (MDTScannedDataFrame *dataframe);

static const GwyEnum frame_types[] = {
    { "Scanned",      MDT_FRAME_SCANNED },
    { "Spectroscopy", MDT_FRAME_SPECTROSCOPY },
    { "Text",         MDT_FRAME_TEXT },
    { "Old MDA",      MDT_FRAME_OLD_MDA },
    { "MDA",          MDT_FRAME_MDA },
    { "Palette",      MDT_FRAME_PALETTE },
};

static const GwyEnum mdt_units[] = {
    { "1/cm", MDT_UNIT_RAMAN_SHIFT },
    { "",     MDT_UNIT_RESERVED0 },
    { "",     MDT_UNIT_RESERVED1 },
    { "",     MDT_UNIT_RESERVED2 },
    { "",     MDT_UNIT_RESERVED3 },
    { "m",    MDT_UNIT_METER },
    { "cm",   MDT_UNIT_CENTIMETER },
    { "mm",   MDT_UNIT_MILLIMETER },
    { "µm",   MDT_UNIT_MIKROMETER },
    { "nm",   MDT_UNIT_NANOMETER },
    { "Å",    MDT_UNIT_ANGSTROM },
    { "nA",   MDT_UNIT_NANOAMPERE },
    { "V",    MDT_UNIT_VOLT },
    { "",     MDT_UNIT_NONE },
    { "kHz",  MDT_UNIT_KILOHERZ },
    { "deg",  MDT_UNIT_DEGREES },
    { "%",    MDT_UNIT_PERCENT },
    { "°C",   MDT_UNIT_CELSIUM_DEGREE },
    { "V",    MDT_UNIT_VOLT_HIGH },
    { "s",    MDT_UNIT_SECOND },
    { "ms",   MDT_UNIT_MILLISECOND },
    { "µs",   MDT_UNIT_MIKROSECOND },
    { "ns",   MDT_UNIT_NANOSECOND },
    { "",     MDT_UNIT_COUNTS },
    { "px",   MDT_UNIT_PIXELS },
    { "",     MDT_UNIT_RESERVED_SFOM0 },
    { "",     MDT_UNIT_RESERVED_SFOM1 },
    { "",     MDT_UNIT_RESERVED_SFOM2 },
    { "",     MDT_UNIT_RESERVED_SFOM3 },
    { "",     MDT_UNIT_RESERVED_SFOM4 },
    { "A",    MDT_UNIT_AMPERE2 },
    { "mA",   MDT_UNIT_MILLIAMPERE },
    { "µA",   MDT_UNIT_MIKROAMPERE },
    { "nA",   MDT_UNIT_NANOAMPERE2 },
    { "pA",   MDT_UNIT_PICOAMPERE },
    { "V",    MDT_UNIT_VOLT2 },
    { "mV",   MDT_UNIT_MILLIVOLT },
    { "µV",   MDT_UNIT_MIKROVOLT },
    { "nV",   MDT_UNIT_NANOVOLT },
    { "pV",   MDT_UNIT_PICOVOLT },
    { "N",    MDT_UNIT_NEWTON },
    { "mN",   MDT_UNIT_MILLINEWTON },
    { "µN",   MDT_UNIT_MIKRONEWTON },
    { "nN",   MDT_UNIT_NANONEWTON },
    { "pN",   MDT_UNIT_PICONEWTON },
    { "",     MDT_UNIT_RESERVED_DOS0 },
    { "",     MDT_UNIT_RESERVED_DOS1 },
    { "",     MDT_UNIT_RESERVED_DOS2 },
    { "",     MDT_UNIT_RESERVED_DOS3 },
    { "",     MDT_UNIT_RESERVED_DOS4 },
};

static const GwyEnum mdt_spm_techniques[] = {
    { "Contact Mode",     MDT_SPM_TECHNIQUE_CONTACT_MODE,     },
    { "Semicontact Mode", MDT_SPM_TECHNIQUE_SEMICONTACT_MODE, },
    { "Tunnel Current",   MDT_SPM_TECHNIQUE_TUNNEL_CURRENT,   },
    { "SNOM",             MDT_SPM_TECHNIQUE_SNOM,             },
};

static const GwyEnum mdt_spm_modes[] = {
    { "Constant Force",               MDT_SPM_MODE_CONSTANT_FORCE,           },
    { "Contact Constant Height",      MDT_SPM_MODE_CONTACT_CONSTANT_HEIGHT,  },
    { "Contact Error",                MDT_SPM_MODE_CONTACT_ERROR,            },
    { "Lateral Force",                MDT_SPM_MODE_LATERAL_FORCE,            },
    { "Force Modulation",             MDT_SPM_MODE_FORCE_MODULATION,         },
    { "Spreading Resistance Imaging", MDT_SPM_MODE_SPREADING_RESISTANCE_IMAGING, },
    { "Semicontact Topography",       MDT_SPM_MODE_SEMICONTACT_TOPOGRAPHY,   },
    { "Semicontact Error",            MDT_SPM_MODE_SEMICONTACT_ERROR,        },
    { "Phase Contrast",               MDT_SPM_MODE_PHASE_CONTRAST,           },
    { "AC Magnetic Force",            MDT_SPM_MODE_AC_MAGNETIC_FORCE,        },
    { "DC Magnetic Force",            MDT_SPM_MODE_DC_MAGNETIC_FORCE,        },
    { "Electrostatic Force",          MDT_SPM_MODE_ELECTROSTATIC_FORCE,      },
    { "Capacitance Contrast",         MDT_SPM_MODE_CAPACITANCE_CONTRAST,     },
    { "Kelvin Probe",                 MDT_SPM_MODE_KELVIN_PROBE,             },
    { "Constant Current",             MDT_SPM_MODE_CONSTANT_CURRENT,         },
    { "Barrier Height",               MDT_SPM_MODE_BARRIER_HEIGHT,           },
    { "Constant Height",              MDT_SPM_MODE_CONSTANT_HEIGHT,          },
    { "AFAM",                         MDT_SPM_MODE_AFAM,                     },
    { "Contact EFM",                  MDT_SPM_MODE_CONTACT_EFM,              },
    { "Shear Force Topography",       MDT_SPM_MODE_SHEAR_FORCE_TOPOGRAPHY,   },
    { "SFOM",                         MDT_SPM_MODE_SFOM,                     },
    { "Contact Capacitance",          MDT_SPM_MODE_CONTACT_CAPACITANCE,      },
    { "SNOM Transmission",            MDT_SPM_MODE_SNOM_TRANSMISSION,        },
    { "SNOM Reflection",              MDT_SPM_MODE_SNOM_REFLECTION,          },
    { "SNOM All",                     MDT_SPM_MODE_SNOM_ALL,                 },
    { "SNOM",                         MDT_SPM_MODE_SNOM,                     },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NT-MDT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.8",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nt-mdt",
                           N_("NT-MDT files (.mdt)"),
                           (GwyFileDetectFunc)&mdt_detect,
                           (GwyFileLoadFunc)&mdt_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mdt_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
mdt_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyContainer *meta, *data = NULL;
    MDTFile mdtfile;
    GString *key;
    guint n, i;

    gwy_debug("");
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    memset(&mdtfile, 0, sizeof(mdtfile));
    if (!mdt_real_load(buffer, size, &mdtfile, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    n = 0;
    data = gwy_container_new();
    key = g_string_new(NULL);
    for (i = 0; i <= mdtfile.last_frame; i++) {
        if (mdtfile.frames[i].type == MDT_FRAME_SCANNED) {
            MDTScannedDataFrame *sdframe;

            sdframe = (MDTScannedDataFrame*)mdtfile.frames[i].frame_data;
            dfield = extract_scanned_data(sdframe);
            g_string_printf(key, "/%d/data", n);
            gwy_container_set_object_by_name(data, key->str, dfield);
            g_object_unref(dfield);

            if (sdframe->title) {
                g_string_append(key, "/title");
                gwy_container_set_string_by_name(data, key->str,
                                                 g_strndup(sdframe->title,
                                                           sdframe->title_len));
            }
            else
                gwy_app_channel_title_fall_back(data, n);

            meta = mdt_get_metadata(&mdtfile, n);
            mdt_add_frame_metadata(sdframe, meta);
            g_string_printf(key, "/%d/meta", n);
            gwy_container_set_object_by_name(data, key->str, meta);
            g_object_unref(meta);

            n++;
        }
    }
    g_string_free(key, TRUE);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!n) {
        g_object_unref(data);
        err_NO_DATA(error);
        return NULL;
    }

    return data;
}

#define HASH_SET_META(fmt, val, key) \
    g_string_printf(s, fmt, val); \
    gwy_container_set_string_by_name(meta, key, g_strdup(s->str))

static GwyContainer*
mdt_get_metadata(MDTFile *mdtfile,
                 guint i)
{
    GwyContainer *meta;
    MDTFrame *frame;
    MDTScannedDataFrame *sdframe;
    GString *s;

    meta = gwy_container_new();

    g_return_val_if_fail(i <= mdtfile->last_frame, meta);
    frame = mdtfile->frames + i;
    g_return_val_if_fail(frame->type == MDT_FRAME_SCANNED, meta);
    sdframe = (MDTScannedDataFrame*)frame->frame_data;

    s = g_string_new(NULL);
    g_string_printf(s, "%d-%02d-%02d %02d:%02d:%02d",
                    frame->year, frame->month, frame->day,
                    frame->hour, frame->min, frame->sec);
    gwy_container_set_string_by_name(meta, "Date", g_strdup(s->str));

    g_string_printf(s, "%d.%d",
                    frame->version/0x100, frame->version % 0x100);
    gwy_container_set_string_by_name(meta, "Version", g_strdup(s->str));

    g_string_printf(s, "%c%c%c%s",
                    (sdframe->scan_dir & 0x02) ? '-' : '+',
                    (sdframe->scan_dir & 0x01) ? 'X' : 'Y',
                    (sdframe->scan_dir & 0x04) ? '-' : '+',
                    (sdframe->scan_dir & 0x80) ? " (double pass)" : "");
    gwy_container_set_string_by_name(meta, "Scan direction", g_strdup(s->str));

    HASH_SET_META("%d", sdframe->channel_index, "Channel index");
    HASH_SET_META("%d", sdframe->mode, "Mode");
    HASH_SET_META("%d", sdframe->ndacq, "Step (DAC)");
    HASH_SET_META("%.2f nm", sdframe->step_length/Nano, "Step length");
    HASH_SET_META("%.0f nm/s", sdframe->velocity/Nano, "Scan velocity");
    HASH_SET_META("%.2f nA", sdframe->setpoint/Nano, "Setpoint value");
    HASH_SET_META("%.2f V", sdframe->bias_voltage, "Bias voltage");

    g_string_free(s, TRUE);

    return meta;
}

static void
mdt_add_frame_metadata(MDTScannedDataFrame *sdframe,
                       GwyContainer *meta)
{
    GMarkupParseContext *context;
    GMarkupParser parser;

    if (!sdframe->xmlstuff)
        return;

    memset(&parser, 0, sizeof(GMarkupParser));
    context = g_markup_parse_context_new(&parser, 0, NULL, NULL);
    g_markup_parse_context_parse(context, sdframe->xmlstuff, -1, NULL);
    g_markup_parse_context_free(context);

    g_free(sdframe->xmlstuff);
    sdframe->xmlstuff = NULL;
}

static void
mdt_read_axis_scales(const guchar *p,
                     MDTAxisScale *x_scale,
                     MDTAxisScale *y_scale,
                     MDTAxisScale *z_scale)
{
    x_scale->offset = gwy_get_gfloat_le(&p);
    x_scale->step = gwy_get_gfloat_le(&p);
    x_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("x: *%g +%g [%d:%s]",
              x_scale->step, x_scale->offset, x_scale->unit,
              gwy_enum_to_string(x_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    x_scale->step = fabs(x_scale->step);
    if (!x_scale->step) {
        g_warning("x_scale.step == 0, changing to 1");
        x_scale->step = 1.0;
    }

    y_scale->offset = gwy_get_gfloat_le(&p);
    y_scale->step = gwy_get_gfloat_le(&p);
    y_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("y: *%g +%g [%d:%s]",
              y_scale->step, y_scale->offset, y_scale->unit,
              gwy_enum_to_string(y_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    y_scale->step = fabs(y_scale->step);
    if (!y_scale->step) {
        g_warning("y_scale.step == 0, changing to 1");
        y_scale->step = 1.0;
    }

    z_scale->offset = gwy_get_gfloat_le(&p);
    z_scale->step = gwy_get_gfloat_le(&p);
    z_scale->unit = (gint16)gwy_get_guint16_le(&p);
    gwy_debug("z: *%g +%g [%d:%s]",
              z_scale->step, z_scale->offset, z_scale->unit,
              gwy_enum_to_string(z_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    if (!z_scale->step) {
        g_warning("z_scale.step == 0, changing to 1");
        z_scale->step = 1.0;
    }
}

static gboolean
mdt_scanned_data_vars(const guchar *p,
                      const guchar *fstart,
                      MDTScannedDataFrame *frame,
                      guint frame_size,
                      guint vars_size,
                      GError **error)
{
    mdt_read_axis_scales(p, &frame->x_scale, &frame->y_scale, &frame->z_scale);
    p += AXIS_SCALES_SIZE;

    frame->channel_index = (gint)(*p++);
    frame->mode = (gint)(*p++);
    frame->xres = gwy_get_guint16_le(&p);
    frame->yres = gwy_get_guint16_le(&p);
    gwy_debug("channel_index = %d, mode = %d, xres = %d, yres = %d",
              frame->channel_index, frame->mode, frame->xres, frame->yres);
    frame->ndacq = gwy_get_guint16_le(&p);
    frame->step_length = Angstrom*gwy_get_gfloat_le(&p);
    frame->adt = gwy_get_guint16_le(&p);
    frame->adc_gain_amp_log10 = (guint)(*p++);
    frame->adc_index = (guint)(*p++);
    frame->s16.version = (guint)(*p++);
    frame->s17.pass_num = (guint)(*p++);
    frame->scan_dir = (guint)(*p++);
    frame->power_of_2 = (gboolean)(*p++);
    frame->velocity = Angstrom*gwy_get_gfloat_le(&p);
    frame->setpoint = Nano*gwy_get_gfloat_le(&p);
    frame->bias_voltage = gwy_get_gfloat_le(&p);
    frame->draw = (gboolean)(*p++);
    p++;
    frame->xoff = gwy_get_gint32_le(&p);
    frame->yoff = gwy_get_gint32_le(&p);
    frame->nl_corr = (gboolean)(*p++);

    p = fstart + FRAME_HEADER_SIZE + vars_size;
    if ((guint)(p - fstart) + FRAME_MODE_SIZE > frame_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Frame is too short for Frame Mode."));
        return FALSE;
    }
    frame->fm_mode = gwy_get_guint16_le(&p);
    frame->fm_xres = gwy_get_guint16_le(&p);
    frame->fm_yres = gwy_get_guint16_le(&p);
    frame->fm_ndots = gwy_get_guint16_le(&p);
    gwy_debug("mode = %u, xres = %u, yres = %u, ndots = %u",
              frame->fm_mode, frame->fm_xres, frame->fm_yres, frame->fm_ndots);

    if ((guint)(p - fstart)
        + sizeof(gint16)*(2*frame->fm_ndots + frame->fm_xres * frame->fm_yres)
        > frame_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Frame is too short for dots or data."));
        return FALSE;
    }

    if (frame->fm_ndots) {
        frame->dots = p;
        p += sizeof(gint16)*2*frame->fm_ndots;
    }
    if (frame->fm_xres * frame->fm_yres) {
        frame->image = p;
        p += sizeof(gint16)*frame->fm_xres*frame->fm_yres;
    }

    gwy_debug("remaining stuff size: %u", (guint)(frame_size - (p - fstart)));

    /* Title */
    if ((frame_size - (p - fstart)) > 4) {
        frame->title_len = gwy_get_guint32_le(&p);
        if (frame->title_len
            && (guint)(frame_size - (p - fstart)) >= frame->title_len) {
            frame->title = p;
            p += frame->title_len;
            gwy_debug("title = <%.*s>", frame->title_len, frame->title);
        }
    }

    /* XML stuff */
    if ((frame_size - (p - fstart)) > 4) {
        guint len = gwy_get_guint32_le(&p);

        if (len && (guint)(frame_size - (p - fstart)) >= len) {
            frame->xmlstuff = g_convert((const gchar*)p, len, "UTF-16", "UTF-8",
                                        NULL, NULL, NULL);
            p += len;
        }
    }

#ifdef DEBUG
    {
        GString *str;
        guint i;

        str = g_string_new(NULL);
        for (i = 0; i < (guint)(frame_size - (p -fstart)); i++) {
            if (g_ascii_isprint(p[i]))
                g_string_append_c(str, p[i]);
            else
                g_string_append_printf(str, "." /*, p[i] */);
        }
        gwy_debug("stuff: %s", str->str);
        g_string_free(str, TRUE);
    }
#endif

    return TRUE;
}

static gboolean
mdt_real_load(const guchar *buffer,
              guint size,
              MDTFile *mdtfile,
              GError **error)
{
    guint i;
    const guchar *p, *fstart;
    MDTScannedDataFrame *scannedframe;

    /* File Header */
    if (size < 32) {
        err_TOO_SHORT(error);
        return FALSE;
    }
    p = buffer + 4;  /* magic header */
    mdtfile->size = gwy_get_guint32_le(&p);
    gwy_debug("File size (w/o header): %u", mdtfile->size);
    p += 4;  /* reserved */
    mdtfile->last_frame = gwy_get_guint16_le(&p);
    gwy_debug("Last frame: %u", mdtfile->last_frame);
    p += 18;  /* reserved */
    /* XXX: documentation specifies 32 bytes long header, but zeroth frame
     * starts at 33th byte in reality */
    p++;

    if (mdtfile->size + 33 != size) {
        err_SIZE_MISMATCH(error, size, mdtfile->size + 32);
        return FALSE;
    }

    /* Frames */
    mdtfile->frames = g_new0(MDTFrame, mdtfile->last_frame + 1);
    for (i = 0; i <= mdtfile->last_frame; i++) {
        MDTFrame *frame = mdtfile->frames + i;

        fstart = p;
        if ((guint)(p - buffer) + FRAME_HEADER_SIZE > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached in frame header #%u."), i);
            return FALSE;
        }
        frame->size = gwy_get_guint32_le(&p);
        gwy_debug("Frame #%u size: %u", i, frame->size);
        if ((guint)(p - buffer) + frame->size - 4 > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached in frame data #%u."), i);
            return FALSE;
        }
        frame->type = gwy_get_guint16_le(&p);
        gwy_debug("Frame #%u type: %s", i,
                  gwy_enum_to_string(frame->type,
                                     frame_types, G_N_ELEMENTS(frame_types)));
        frame->version = ((guint)p[0] << 8) + (gsize)p[1];
        p += 2;
        gwy_debug("Frame #%u version: %d.%d",
                  i, frame->version/0x100, frame->version % 0x100);
        frame->year = gwy_get_guint16_le(&p);
        frame->month = gwy_get_guint16_le(&p);
        frame->day = gwy_get_guint16_le(&p);
        frame->hour = gwy_get_guint16_le(&p);
        frame->min = gwy_get_guint16_le(&p);
        frame->sec = gwy_get_guint16_le(&p);
        gwy_debug("Frame #%u datetime: %d-%02d-%02d %02d:%02d:%02d",
                  i, frame->year, frame->month, frame->day,
                  frame->hour, frame->min, frame->sec);
        frame->var_size = gwy_get_guint16_le(&p);
        gwy_debug("Frame #%u var size: %u", i, frame->var_size);
        if (frame->var_size + FRAME_HEADER_SIZE > frame->size) {
            err_SIZE_MISMATCH(error, frame->var_size + FRAME_HEADER_SIZE,
                              frame->size);
            return FALSE;
        }

        switch (frame->type) {
            case MDT_FRAME_SCANNED:
            if (frame->var_size < AXIS_SCALES_SIZE + SCAN_VARS_MIN_SIZE) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Frame #%u is too short for "
                              "scanned data header."), i);
                return FALSE;
            }
            scannedframe = g_new0(MDTScannedDataFrame, 1);
            if (!mdt_scanned_data_vars(p, fstart, scannedframe,
                                       frame->size, frame->var_size, error))
                return FALSE;
            frame->frame_data = scannedframe;
            break;

            case MDT_FRAME_SPECTROSCOPY:
            gwy_debug("Spectroscropy frames make little sense to read now");
            break;

            case MDT_FRAME_TEXT:
            gwy_debug("Cannot read text frame");
            /*
            p = fstart + FRAME_HEADER_SIZE + frame->var_size;
            p += 16;
            for (j = 0; j < frame->size - (p - fstart); j++)
                g_print("%c", g_ascii_isprint(p[j]) ? p[j] : '.');
            g_printerr("%s\n", g_convert(p, frame->size - (p - fstart),
                                         "UCS-2", "UTF-8", NULL, &j, NULL));
                                         */
            break;

            case MDT_FRAME_OLD_MDA:
            gwy_debug("Cannot read old MDA frame");
            break;

            case MDT_FRAME_MDA:
            gwy_debug("Cannot read MDA frame");
            break;

            case MDT_FRAME_PALETTE:
            gwy_debug("Cannot read palette frame");
            break;

            default:
            g_warning("Unknown frame type %d", frame->type);
            break;
        }

        p = fstart + frame->size;
    }

    return TRUE;
}

static GwyDataField*
extract_scanned_data(MDTScannedDataFrame *dataframe)
{
    GwyDataField *dfield;
    GwySIUnit *siunitxy, *siunitz;
    guint i;
    gdouble *data;
    gdouble xreal, yreal, zscale;
    gint power10xy, power10z;
    const gint16 *p;
    const gchar *unit;

    if (dataframe->x_scale.unit != dataframe->y_scale.unit)
        g_warning("Different x and y units, using x for both (incorrect).");
    unit = gwy_enum_to_string(dataframe->x_scale.unit,
                              mdt_units, G_N_ELEMENTS(mdt_units));
    siunitxy = gwy_si_unit_new_parse(unit, &power10xy);
    xreal = dataframe->fm_xres*pow10(power10xy)*dataframe->x_scale.step;
    yreal = dataframe->fm_yres*pow10(power10xy)*dataframe->y_scale.step;

    unit = gwy_enum_to_string(dataframe->z_scale.unit,
                              mdt_units, G_N_ELEMENTS(mdt_units));
    siunitz = gwy_si_unit_new_parse(unit, &power10z);
    zscale = pow10(power10z)*dataframe->z_scale.step;

    dfield = gwy_data_field_new(dataframe->fm_xres, dataframe->fm_yres,
                                xreal, yreal,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunitxy);
    g_object_unref(siunitxy);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitz);

    data = gwy_data_field_get_data(dfield);
    p = (gint16*)dataframe->image;
    for (i = 0; i < dataframe->fm_xres*dataframe->fm_yres; i++)
        data[i] = zscale*GINT16_FROM_LE(p[i]);

    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
