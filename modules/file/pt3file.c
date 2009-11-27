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

/* FIXME: I don't have any v5.0 files (those with TimeHarp magic header) so
 * we don't try to read them. */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-symphotime-pt3">
 *   <comment>SymPhoTime TTTR v2.0 data</comment>
 *   <magic priority="90">
 *     <match type="string" offset="0" value="PicoHarp 300"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * SymPhoTime TTTR v2.0 data
 * .pt3
 * Read
 **/
#define DEBUG 1
#include <string.h>
#include <stdio.h>
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "PicoHarp 300"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".pt3"

enum {
    HEADER_MIN_SIZE = 728 + 8,   /* 8 for dimensions + instrument */
    BOARD_SIZE = 150,
    CRLF_OFFSET = 70,
    IMAGING_PIE710_SIZE = 60,
    IMAGING_KDT180_SIZE = 44,
    IMAGING_LSM_SIZE = 32,
};

typedef enum {
    PICO_HARP_TIMED = 0,
    PICO_HARP_IMAGE = 3,
} PicoHarpSubMode;

typedef enum {
    PICO_HARP_STOP_TIME_OVER = 0,
    PICO_HARP_STOP_MANUAL    = 1,
    PICO_HARP_STOP_OVERFLOW  = 2,
    PICO_HARP_STOP_ERROR     = 3,
} PicoHarpStopReason;

typedef enum {
    PICO_HARP_INPUT_CUSTOM = 0,
    PICO_HARP_INPUT_NIM    = 1,
    PICO_HARP_INPUT_TTL    = 2,
} PicoHarpInputType;

typedef enum {
    PICO_HARP_INPUT_FALLING = 0,
    PICO_HARP_INPUT_RISING  = 1,
} PicoHarpInputEdge;

typedef enum {
    PICO_HARP_PIE710 = 1,
    PICO_HARP_KDT180 = 2,
    PICO_HARP_LSM    = 3,
} PicoHarpInstrument;

typedef struct {
    guint channel;
    guint time;
} PicoHarpT2Record;

typedef struct {
    guint channel;
    guint time;
    guint nsync;
} PicoHarpT3Record;

typedef struct {
    guint map_to;
    guint show;
} PicoHarpDisplayCurve;

typedef struct {
    gdouble start;
    gdouble step;
    gdouble end;
} PicoHarpAutomatedParam;

typedef struct {
    PicoHarpInputType input_type;
    gint input_level;         /* in mV */
    PicoHarpInputEdge input_edge;
    gboolean cfd_present;
    gint cfd_level;           /* in mV */
    gint cfd_zero_cross;      /* in mV */
} PicoHarpRTChannel;

typedef struct {
    gchar hardware_ident[16];    /* the docs say 10 but it is clearly 0x10 */
    gchar hardware_version[8];
    guint hardware_serial;
    guint sync_divider;
    guint cfd_zero_cross0;
    guint cfd_level0;
    guint cfd_zero_cross1;
    guint cfd_level1;
    gdouble resolution;
    guint router_model_code;
    gboolean router_enabled;
    PicoHarpRTChannel rt_channel[4];
} PicoHarpBoard;

typedef struct {
    guint dimensions;
    PicoHarpInstrument instrument;
    guint xres;
    guint yres;
    gboolean bidirectional;
} PicoHarpCommonImagingHeader;

/* XXX: The fields are not in file order here, we try to maximize the number
 * of items in common. */
typedef struct {
    guint dimensions;
    PicoHarpInstrument instrument;
    guint xres;
    guint yres;
    gboolean bidirectional;
    guint time_per_pixel;    /* [0.2ms] */
    guint acceleration;      /* interval length in % of total width */
    guint reserved;
    gdouble xoff;            /* [µm] */
    gdouble yoff;            /* [µm] */
    gdouble pix_resol;       /* [µm/px] */
    gdouble t_start_to;
    gdouble t_stop_to;
    gdouble t_start_from;
    gdouble t_stop_from;
} PicoHarpPIE710ImagingHeader;

typedef struct {
    guint dimensions;
    PicoHarpInstrument instrument;
    guint xres;
    guint yres;
    gboolean bidirectional;
    guint velocity;          /* [ticks/s] */
    guint acceleration;      /* [ticks/s²] */
    guint reserved;
    gdouble xoff;            /* [µm] */
    gdouble yoff;            /* [µm] */
    gdouble pix_resol;       /* [µm/px] */
} PicoHarpKDT180ImagingHeader;

typedef struct {
    guint dimensions;
    PicoHarpInstrument instrument;
    guint xres;
    guint yres;
    gboolean bidirectional;
    guint frame_trigger_index;
    guint line_start_trigger_index;
    guint line_stop_trigger_index;
} PicoHarpLSMImagingHeader;

typedef union {
    PicoHarpCommonImagingHeader common;
    PicoHarpPIE710ImagingHeader pie710;
    PicoHarpKDT180ImagingHeader kdt180;
    PicoHarpLSMImagingHeader lsm;
} PicoHarpImagingHeader;

typedef struct {
    gchar ident[16];
    gchar format_version[6];
    gchar creator_name[18];
    gchar creator_version[12];
    gchar file_time[18];
    gchar crlf[2];
    gchar comment[256];
    guint number_of_curves;
    guint bits_per_record;
    guint routing_channels;
    guint number_of_boards;
    guint active_curve;
    guint measurement_mode;
    PicoHarpSubMode sub_mode;
    guint range_no;
    gint offset;            /* [ns] */
    guint acquisition_time; /* [ms] */
    guint stop_at;          /* meaningless in TTTR mode */
    guint stop_on_ovfl;     /* meaningless in TTTR mode */
    guint restart;          /* meaningless in TTTR mode */
    gboolean display_lin_log;
    guint display_time_axis_from;
    guint display_time_axis_to;
    guint display_counts_axis_from;
    guint display_counts_axis_to;
    PicoHarpDisplayCurve display_curve[8];    /* meaningless in TTTR mode */
    PicoHarpAutomatedParam auto_param[3];
    guint repeat_mode;
    guint repeats_per_curve;
    guint repeat_time;
    guint repeat_wait_time;
    gchar script_name[20];
    PicoHarpBoard board;   /* the length is number_of_boards, however, it
                              must be 1 so we ignore any possible boards
                              after that */
    guint ext_devices;     /* bit field */
    guint reserved1;
    guint reserved2;
    guint input0_rate;
    guint input1_rate;
    guint stop_after;
    PicoHarpStopReason stop_reason;
    guint number_of_records;
    guint spec_header_length;    /* stored value has units [4bytes], but we
                                    recalculate it upon reading */
    PicoHarpImagingHeader imaging;
} PicoHarpFile;

typedef struct {
    guint i;
    guint64 globaltime;
    guint64 start, stop;
} LineTrigger;

static gboolean      module_register           (void);
static gint          pt3file_detect            (const GwyFileDetectInfo *fileinfo,
                                                gboolean only_name);
static GwyContainer* pt3file_load              (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static gsize         pt3file_read_header       (const guchar *buffer,
                                                gsize size,
                                                PicoHarpFile *pt3file,
                                                GError **error);
static const guchar* pt3file_read_board        (PicoHarpBoard *board,
                                                const guchar *p);
static const guchar* read_pie710_imaging_header(PicoHarpImagingHeader *header,
                                                const guchar *p);
static const guchar* read_kdt180_imaging_header(PicoHarpImagingHeader *header,
                                                const guchar *p);
static const guchar* read_lsm_imaging_header   (PicoHarpImagingHeader *header,
                                                const guchar *p);
static LineTrigger*  pt3file_scan_line_triggers(const PicoHarpFile *pt3file,
                                                const guchar *p,
                                                GError **error);
static GwyDataField* pt3file_extract_counts    (const PicoHarpFile *pt3file,
                                                const LineTrigger *linetriggers,
                                                const guchar *p);
static GwyContainer* pt3file_get_metadata      (PicoHarpFile *pt3file);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports SymPhoTime data files, version 2.0."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("pt3file",
                           N_("PicoHarp files (.pt3)"),
                           (GwyFileDetectFunc)&pt3file_detect,
                           (GwyFileLoadFunc)&pt3file_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
pt3file_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 30 : 0;

    if (fileinfo->buffer_len < HEADER_MIN_SIZE)
        return 0;

    if (memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && fileinfo->head[CRLF_OFFSET] == '\r'
        && fileinfo->head[CRLF_OFFSET+1] == '\n')
        score = 100;

    return score;
}

static inline const guchar*
read_t2_record(PicoHarpT2Record *rec,
               const guchar *p)
{
    guint32 r = gwy_get_guint32_le(&p);

    rec->time = r & 0x0fffffffU;
    rec->channel = r >> 28;

    return p;
}

static inline const guchar*
read_t3_record(PicoHarpT3Record *rec,
               const guchar *p)
{
    guint32 r = gwy_get_guint32_le(&p);

    rec->nsync = r & 0x0000ffffU;
    r = r >> 16;
    rec->time = r & 0x000000fffU;
    rec->channel = r >> 12;

    return p;
}

static GwyContainer*
pt3file_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    PicoHarpFile pt3file;
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize header_len, size = 0;
    GError *err = NULL;
    LineTrigger *linetriggers = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(&pt3file, 1);
    if (!(header_len = pt3file_read_header(buffer, size, &pt3file, error)))
        goto fail;

    if (err_SIZE_MISMATCH
        (error, header_len + pt3file.number_of_records*sizeof(guint32), size,
         FALSE))
        goto fail;

    /* Scan the records and find the line triggers */
    p = buffer + header_len;
    if (!(linetriggers = pt3file_scan_line_triggers(&pt3file, p, error)))
        goto fail;

    dfield = pt3file_extract_counts(&pt3file, linetriggers, p);
    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

fail:
    g_free(linetriggers);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gsize
pt3file_read_header(const guchar *buffer,
                    gsize size,
                    PicoHarpFile *pt3file,
                    GError **error)
{
    const guchar* (*read_imaging_header)(PicoHarpImagingHeader *header,
                                         const guchar *p);
    PicoHarpInstrument instr;
    const guchar *p;
    guint i, expected_size;

    p = buffer;
    if (size < HEADER_MIN_SIZE + 2) {
        err_TOO_SHORT(error);
        return 0;
    }

    get_CHARARRAY(pt3file->ident, &p);
    get_CHARARRAY(pt3file->format_version, &p);
    get_CHARARRAY(pt3file->creator_name, &p);
    get_CHARARRAY(pt3file->creator_version, &p);
    gwy_debug("<%.*s> <%.*s> <%.*s> <%.*s>",
              (gint)sizeof(pt3file->ident), pt3file->ident,
              (gint)sizeof(pt3file->format_version), pt3file->format_version,
              (gint)sizeof(pt3file->creator_name), pt3file->creator_name,
              (gint)sizeof(pt3file->creator_version), pt3file->creator_version);
    get_CHARARRAY(pt3file->file_time, &p);
    get_CHARARRAY(pt3file->crlf, &p);
    get_CHARARRAY(pt3file->comment, &p);
    if (memcmp(pt3file->ident, MAGIC, MAGIC_SIZE) != 0
        || pt3file->crlf[0] != '\r'
        || pt3file->crlf[1] != '\n') {
        err_FILE_TYPE(error, "PicoHarp");
        return 0;
    }

    pt3file->number_of_curves = gwy_get_guint32_le(&p);
    gwy_debug("number_of_curves: %u", pt3file->number_of_curves);
    pt3file->bits_per_record = gwy_get_guint32_le(&p);
    gwy_debug("bits_per_record: %u", pt3file->bits_per_record);
    pt3file->routing_channels = gwy_get_guint32_le(&p);
    gwy_debug("routing_channels: %u", pt3file->routing_channels);
    pt3file->number_of_boards = gwy_get_guint32_le(&p);
    gwy_debug("number_of_boards: %u", pt3file->number_of_boards);
    if (pt3file->number_of_boards != 1) {
        g_warning("Number of boards is %u instead of 1.  Reading one.",
                  pt3file->number_of_boards);

        pt3file->number_of_boards = MAX(pt3file->number_of_boards, 1);
        if (size < HEADER_MIN_SIZE + BOARD_SIZE*pt3file->number_of_boards) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File header is truncated"));
            return 0;
        }
    }
    pt3file->active_curve = gwy_get_guint32_le(&p);
    pt3file->measurement_mode = gwy_get_guint32_le(&p);
    gwy_debug("measurement_mode: %u", pt3file->measurement_mode);
    pt3file->sub_mode = gwy_get_guint32_le(&p);
    gwy_debug("sub_mode: %u", pt3file->sub_mode);
    pt3file->range_no = gwy_get_guint32_le(&p);
    pt3file->offset = gwy_get_gint32_le(&p);
    pt3file->acquisition_time = gwy_get_guint32_le(&p);
    pt3file->stop_at = gwy_get_guint32_le(&p);
    pt3file->stop_on_ovfl = gwy_get_guint32_le(&p);
    pt3file->restart = gwy_get_guint32_le(&p);
    pt3file->display_lin_log = !!gwy_get_guint32_le(&p);
    pt3file->display_time_axis_from = gwy_get_guint32_le(&p);
    pt3file->display_time_axis_to = gwy_get_guint32_le(&p);
    pt3file->display_counts_axis_from = gwy_get_guint32_le(&p);
    pt3file->display_counts_axis_to = gwy_get_guint32_le(&p);
    for (i = 0; i < G_N_ELEMENTS(pt3file->display_curve); i++) {
        pt3file->display_curve[i].map_to = gwy_get_guint32_le(&p);
        pt3file->display_curve[i].show = gwy_get_guint32_le(&p);
    }
    for (i = 0; i < G_N_ELEMENTS(pt3file->auto_param); i++) {
        pt3file->auto_param[i].start = gwy_get_gfloat_le(&p);
        pt3file->auto_param[i].step = gwy_get_gfloat_le(&p);
        pt3file->auto_param[i].end = gwy_get_gfloat_le(&p);
    }
    pt3file->repeat_mode = gwy_get_guint32_le(&p);
    pt3file->repeats_per_curve = gwy_get_guint32_le(&p);
    pt3file->repeat_time = gwy_get_guint32_le(&p);
    pt3file->repeat_wait_time = gwy_get_guint32_le(&p);
    get_CHARARRAY(pt3file->script_name, &p);
    p = pt3file_read_board(&pt3file->board, p);
    p += BOARD_SIZE*(pt3file->number_of_boards - 1);
    pt3file->ext_devices = gwy_get_guint32_le(&p);
    pt3file->reserved1 = gwy_get_guint32_le(&p);
    pt3file->reserved2 = gwy_get_guint32_le(&p);
    pt3file->input0_rate = gwy_get_guint32_le(&p);
    pt3file->input1_rate = gwy_get_guint32_le(&p);
    pt3file->stop_after = gwy_get_guint32_le(&p);
    pt3file->stop_reason = gwy_get_guint32_le(&p);
    pt3file->number_of_records = gwy_get_guint32_le(&p);
    gwy_debug("number_of_records: %u", pt3file->number_of_records);
    pt3file->spec_header_length = 4*gwy_get_guint32_le(&p);
    gwy_debug("spec_header_length: %u", pt3file->spec_header_length);
    gwy_debug("now at pos 0x%0lx", (gulong)(p - buffer));

    if (pt3file->measurement_mode != 2
        && pt3file->measurement_mode != 3) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Measurement mode must be 2 or 3; %u is invalid."),
                    pt3file->measurement_mode);
        return 0;
    }
    if (pt3file->sub_mode != PICO_HARP_IMAGE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only area imaging files are supported."));
        return 0;
    }
    if (pt3file->bits_per_record != 32) {
        err_BPP(error, pt3file->bits_per_record);
        return 0;
    }

    pt3file->imaging.common.dimensions = gwy_get_guint32_le(&p);
    gwy_debug("imaging dimensions: %u", pt3file->imaging.common.dimensions);
    if (pt3file->imaging.common.dimensions != 3) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only area imaging files are supported."));
        return 0;
    }
    pt3file->imaging.common.instrument = instr = gwy_get_guint32_le(&p);
    gwy_debug("imaging instrument: %u", pt3file->imaging.common.instrument);
    if (instr == PICO_HARP_PIE710) {
        expected_size = IMAGING_PIE710_SIZE;
        read_imaging_header = &read_pie710_imaging_header;
    }
    else if (instr == PICO_HARP_KDT180) {
        expected_size = IMAGING_KDT180_SIZE;
        read_imaging_header = &read_kdt180_imaging_header;
    }
    else if (instr == PICO_HARP_LSM) {
        expected_size = IMAGING_LSM_SIZE;
        read_imaging_header = &read_lsm_imaging_header;
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid or unsupported instrument number %u."),
                    instr);
        return 0;
    }

    if (pt3file->spec_header_length != expected_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Wrong imagining header size: %u instead of %u."),
                    pt3file->spec_header_length, expected_size);
        return 0;
    }
    if ((p - buffer) + expected_size > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        return 0;
    }

    p = read_imaging_header(&pt3file->imaging, p);
    gwy_debug("xres: %u", pt3file->imaging.common.xres);
    gwy_debug("yres: %u", pt3file->imaging.common.yres);

    if (err_DIMENSION(error, pt3file->imaging.common.xres)
        || err_DIMENSION(error, pt3file->imaging.common.xres))
        return 0;

    return (gsize)(p - buffer);
}

static const guchar*
pt3file_read_board(PicoHarpBoard *board,
                   const guchar *p)
{
    guint i;

    get_CHARARRAY(board->hardware_ident, &p);
    get_CHARARRAY(board->hardware_version, &p);
    gwy_debug("<%.*s> <%.*s>",
              (gint)sizeof(board->hardware_ident), board->hardware_ident,
              (gint)sizeof(board->hardware_version), board->hardware_version);
    board->hardware_serial = gwy_get_guint32_le(&p);
    board->sync_divider = gwy_get_guint32_le(&p);
    board->cfd_zero_cross0 = gwy_get_guint32_le(&p);
    board->cfd_level0 = gwy_get_guint32_le(&p);
    board->cfd_zero_cross1 = gwy_get_guint32_le(&p);
    board->cfd_level1 = gwy_get_guint32_le(&p);
    board->resolution = gwy_get_gfloat_le(&p);
    board->router_model_code = gwy_get_guint32_le(&p);
    board->router_enabled = !!gwy_get_guint32_le(&p);
    for (i = 0; i < G_N_ELEMENTS(board->rt_channel); i++) {
        board->rt_channel[i].input_type = gwy_get_guint32_le(&p);
        board->rt_channel[i].input_level = gwy_get_gint32_le(&p);
        board->rt_channel[i].input_edge = gwy_get_guint32_le(&p);
        board->rt_channel[i].cfd_present = !!gwy_get_guint32_le(&p);
        board->rt_channel[i].cfd_level = gwy_get_gint32_le(&p);
        board->rt_channel[i].cfd_zero_cross = gwy_get_gint32_le(&p);
    }

    return p;
}

static const guchar*
read_pie710_imaging_header(PicoHarpImagingHeader *header,
                           const guchar *p)
{
    header->pie710.time_per_pixel = gwy_get_guint32_le(&p);
    header->pie710.acceleration = gwy_get_guint32_le(&p);
    header->pie710.bidirectional = !!gwy_get_guint32_le(&p);
    header->pie710.reserved = gwy_get_guint32_le(&p);
    header->pie710.xoff = gwy_get_gfloat_le(&p);
    header->pie710.yoff = gwy_get_gfloat_le(&p);
    header->pie710.xres = gwy_get_guint32_le(&p);
    header->pie710.yres = gwy_get_guint32_le(&p);
    header->pie710.pix_resol = gwy_get_gfloat_le(&p);
    header->pie710.t_start_to = gwy_get_gfloat_le(&p);
    header->pie710.t_stop_to = gwy_get_gfloat_le(&p);
    header->pie710.t_start_from = gwy_get_gfloat_le(&p);
    header->pie710.t_stop_from = gwy_get_gfloat_le(&p);
    gwy_debug("%g %g %g %g",
              header->pie710.t_start_to, header->pie710.t_stop_to,
              header->pie710.t_start_from, header->pie710.t_stop_from);

    return p;
}

static const guchar*
read_kdt180_imaging_header(PicoHarpImagingHeader *header,
                           const guchar *p)
{
    header->kdt180.velocity = gwy_get_guint32_le(&p);
    header->kdt180.acceleration = gwy_get_guint32_le(&p);
    header->kdt180.bidirectional = !!gwy_get_guint32_le(&p);
    header->kdt180.reserved = gwy_get_guint32_le(&p);
    header->kdt180.xoff = gwy_get_gfloat_le(&p);
    header->kdt180.yoff = gwy_get_gfloat_le(&p);
    header->kdt180.xres = gwy_get_guint32_le(&p);
    header->kdt180.yres = gwy_get_guint32_le(&p);
    header->kdt180.pix_resol = gwy_get_gfloat_le(&p);

    return p;
}

static const guchar*
read_lsm_imaging_header(PicoHarpImagingHeader *header,
                        const guchar *p)
{
    header->lsm.frame_trigger_index = gwy_get_guint32_le(&p);
    header->lsm.line_start_trigger_index = gwy_get_guint32_le(&p);
    header->lsm.line_stop_trigger_index = gwy_get_guint32_le(&p);
    header->lsm.bidirectional = !!gwy_get_guint32_le(&p);
    header->lsm.xres = gwy_get_guint32_le(&p);
    header->lsm.yres = gwy_get_guint32_le(&p);

    return p;
}

static LineTrigger*
pt3file_scan_line_triggers(const PicoHarpFile *pt3file,
                           const guchar *p,
                           GError **error)
{
    LineTrigger *linetriggers;
    guint64 globaltime, globalbase;
    guint xres, yres, i, lineno, n;

    globaltime = globalbase = 0;
    xres = pt3file->imaging.common.xres;
    yres = pt3file->imaging.common.yres;
    linetriggers = g_new(LineTrigger, yres);
    n = pt3file->number_of_records;
    for (i = lineno = 0; i < n; i++) {
        PicoHarpT3Record rec;

        p = read_t3_record(&rec, p);
        if (rec.channel == 15 && rec.nsync == 0 && rec.time == 0) {
            globalbase += 0x10000;
            continue;
        }
        globaltime = globalbase | rec.nsync;
        if (rec.channel == 15) {
            if (rec.time == 4) {
                if (lineno < yres) {
                    linetriggers[lineno].i = i;
                    linetriggers[lineno].globaltime = globaltime;
                }
                lineno++;
            }
        }
    }

    if (lineno != yres) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Number of line triggers %u does not match the number "
                      "of scan lines %u."),
                    lineno, yres);
        g_free(linetriggers);
        return NULL;
    }

    if (pt3file->imaging.common.instrument == PICO_HARP_PIE710) {
        gdouble start = pt3file->imaging.pie710.t_start_to;
        gdouble stop = pt3file->imaging.pie710.t_stop_to;
        for (lineno = 0; lineno < yres; lineno++) {
            n = (lineno == yres-1) ? lineno-1 : lineno;
            n = linetriggers[n+1].globaltime - linetriggers[n].globaltime;
            linetriggers[lineno].start = (linetriggers[lineno].globaltime
                                          + start*n);
            linetriggers[lineno].stop = (linetriggers[lineno].globaltime
                                         + stop*n);
        }
    }
    else {
        for (lineno = 0; lineno < yres; lineno++) {
            /* XXX: off-by-1 crash */
            linetriggers[lineno].start = linetriggers[lineno].globaltime;
            linetriggers[lineno].stop = linetriggers[lineno+1].globaltime;
        }
    }

    return linetriggers;
}

static GwyDataField*
pt3file_extract_counts(const PicoHarpFile *pt3file,
                       const LineTrigger *linetriggers,
                       const guchar *p)
{
    GwyDataField *dfield;
    guint xres, yres, i, lineno, pixno, n;
    guint64 globaltime, globalbase;
    gdouble pix_resol;
    gdouble *d;

    xres = pt3file->imaging.common.xres;
    yres = pt3file->imaging.common.yres;
    n = pt3file->number_of_records;
    if (pt3file->imaging.common.instrument == PICO_HARP_PIE710)
        pix_resol = pt3file->imaging.pie710.pix_resol;
    else if (pt3file->imaging.common.instrument == PICO_HARP_KDT180)
        pix_resol = pt3file->imaging.kdt180.pix_resol;
    else {
        g_return_val_if_reached(NULL);
    }
    pix_resol *= 1e-6;

    dfield = gwy_data_field_new(xres, yres, pix_resol*xres, pix_resol*yres,
                                TRUE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    d = gwy_data_field_get_data(dfield);

    for (i = lineno = 0; i < n; i++) {
        PicoHarpT3Record rec;

        p = read_t3_record(&rec, p);
        if (rec.channel == 15) {
            if (rec.nsync == 0 && rec.time == 0)
                globalbase += 0x10000;
            continue;
        }
        globaltime = globalbase | rec.nsync;
        while (lineno < yres && globaltime >= linetriggers[lineno].stop)
            lineno++;
        if (lineno == yres)
            break;

        if (globaltime >= linetriggers[lineno].start
            && globaltime < linetriggers[lineno].stop) {
            pixno = (xres*(globaltime - linetriggers[lineno].start)
                     /(linetriggers[lineno].stop - linetriggers[lineno].start));
            pixno = MIN(pixno, xres-1);
            d[xres*lineno + pixno] += 1.0;
        }
    }

    return dfield;
}

static GwyContainer*
pt3file_get_metadata(PicoHarpFile *pt3file)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
