/*
 *  $Id: metropro.c 10195 2009-10-01 23:04:26Z yeti-dn $
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
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "PicoHarp 300"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".pt3"

enum {
    HEADER_MIN_SIZE = 722,
    BOARD_SIZE = 150,
    CRLF_OFFSET = 0x46,
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
    guint spec_header_length;
} PicoHarpFile;

static gboolean      module_register      (void);
static gint          pt3file_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* pt3file_load        (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      pt3file_read_header (const guchar *buffer,
                                           gsize size,
                                           PicoHarpFile *pt3file,
                                           GError **error);
static const guchar* pt3file_read_board(PicoHarpBoard *board,
                                        const guchar *p);
static GwyContainer* pt3file_get_metadata(PicoHarpFile *pt3file);

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

static GwyContainer*
pt3file_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    PicoHarpFile pt3file;
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!pt3file_read_header(buffer, size, &pt3file, error))
        return NULL;

    /*
    expected = pt3file.header_size
               + 2*pt3file.nbuckets*pt3file.intens_xres*pt3file.intens_yres
               + 4*pt3file.phase_xres*pt3file.phase_yres;
    if (err_SIZE_MISMATCH(error, expected, size, TRUE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!n) {
        err_NO_DATA(error);
        return NULL;
    }

    key = g_string_new(NULL);
    */

    container = gwy_container_new();

    return container;
}

static gboolean
pt3file_read_header(const guchar *buffer,
                    gsize size,
                    PicoHarpFile *pt3file,
                    GError **error)
{
    const guchar *p;
    guint i;

    p = buffer;
    if (size < HEADER_MIN_SIZE + 2) {
        err_TOO_SHORT(error);
        return FALSE;
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
        return FALSE;
    }

    pt3file->number_of_curves = gwy_get_guint32_le(&p);
    pt3file->bits_per_record = gwy_get_guint32_le(&p);
    pt3file->routing_channels = gwy_get_guint32_le(&p);
    pt3file->number_of_boards = gwy_get_guint32_le(&p);
    if (pt3file->number_of_boards != 1) {
        g_warning("Number of boards is %u instead of 1.  Reading one.",
                  pt3file->number_of_boards);

        pt3file->number_of_boards = MAX(pt3file->number_of_boards, 1);
        if (size < HEADER_MIN_SIZE + BOARD_SIZE*pt3file->number_of_boards) {
            err_TOO_SHORT(error);
            return FALSE;
        }
    }
    pt3file->active_curve = gwy_get_guint32_le(&p);
    pt3file->measurement_mode = gwy_get_guint32_le(&p);
    pt3file->sub_mode = gwy_get_guint32_le(&p);
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
    pt3file->spec_header_length = gwy_get_guint32_le(&p);
    gwy_debug("spec_header_length: %u", pt3file->spec_header_length);

    return TRUE;
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

static GwyContainer*
pt3file_get_metadata(PicoHarpFile *pt3file)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
