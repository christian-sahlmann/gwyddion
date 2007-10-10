/*
 *  $Id$
 *  Copyright (C) 2004 Rok Zitko
 *  E-mail: rok.zitko@ijs.si
 *
 *  Information on the format was published in the manual
 *  for STMPRG, Copyright (C) 1989-1992 Omicron.
 *
 *  Based on nanoscope.c, Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
 * <mime-type type="application/x-stmprg-spm">
 *   <comment>Omicron STMPRG data parameters</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="MPAR"/>
 *   </magic>
 *   <glob pattern="tp*"/>
 *   <glob pattern="TP*"/>
 * </mime-type>
 **/

/* TODO
 * - store both directions
 * - other channels
 * - height vs. current, sol_z vs. sol_h etc.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC_TXT "MPAR"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

#define Angstrom (1e-10)

typedef enum {
    STMPRG_CHANNEL_OFF  = 0,
    STMPRG_CHANNEL_Z    = 1,
    STMPRG_CHANNEL_I    = 2,
    STMPRG_CHANNEL_I_I0 = 3,
    STMPRG_CHANNEL_EXT1 = 4,
    STMPRG_CHANNEL_EXT2 = 5,
    STMPRG_CHANNEL_U0   = 6,
    STMPRG_CHANNEL_I0   = 7
} StmprgChannelType;

typedef enum {
    STMPRG_SPECTRO_CHANNEL_OFF    = 0,
    STMPRG_SPECTRO_CHANNEL_IZ     = 1,
    STMPRG_SPECTRO_CHANNEL_IU0    = 2,
    STMPRG_SPECTRO_CHANNEL_EXT1Z  = 3,
    STMPRG_SPECTRO_CHANNEL_EXT1U0 = 4,
    STMPRG_SPECTRO_CHANNEL_EXT1I0 = 5
} StmprgSpectroChannelType;

enum {
    MAINFIELD_SIZE     = 60,
    CONTROL_SIZE       = 224,
    OTHER_CONTROL_SIZE = 268,
    PARAM_SIZE         = MAGIC_SIZE + MAINFIELD_SIZE + CONTROL_SIZE
                         + OTHER_CONTROL_SIZE
};

typedef struct {
   gdouble start_x; /* zeropoint of measurment piece coordinates */
   gdouble start_y;
   gdouble field_x; /* length of field for scanning in Angstrom */
   gdouble field_y;
   gdouble inc_x; /* increment steps for point and line (x and y) */
   gdouble inc_y; /* in Angstrom */
   gint points;
   gint lines;
   gdouble angle; /* angle of field in start system */
   gdouble sol_x; /* resolution of x,y,z in Angstrom per Bit */
   gdouble sol_y;
   gdouble sol_z;
   gdouble sol_ext1; /* Ext input 1 */
   gdouble sol_ext2; /* Ext input 2 */
   gdouble sol_h; /* resolution dh for z(i) spectroscopy */
} StmprgMainfield;

typedef struct {
   guint mode; /* on/off bits: 3: Spectr, 4-6: Loop1-3 */
   StmprgChannelType channel1;
   StmprgChannelType channel2;
   StmprgChannelType channel3;
   StmprgSpectroChannelType spectr;
   gint cfree;
   gint type; /* E = 0 or D != 0 - scanning */
   gint steps_x; /* number of steps per increment */
   gint steps_y;
   gint dac_speed; /* slewrate of DAC */
   gdouble poi_inc; /* increment per step in x/y-direction for lines/points */
   gdouble lin_inc; /* in piezo coordinates */
   gint ad1_reads; /* Number of ADC 1 average */
   gint ad2_reads; /* Number of ADC 2 average */
   gint ad3_reads; /* ditto ADC 3 */
   gint analog_ave; /* analogous averaging */
   gint speed; /* speed for z-scan */
   gdouble voltage; /* normal tunnel voltage */
   gdouble voltage_l; /* voltage for scanning left */
   gdouble voltage_r; /* voltage for scanning right */
   gint volt_flag; /* 0 = internal, 1 = remote */
   gint volt_region; /* region */
   gdouble current; /* normal tunnel current */
   gdouble current_l; /* current for scanning left */
   gdouble current_r; /* current for scanning right */
   gint curr_flag; /* 0 = internal, 1 = remote */
   gint curr_region; /* region */
   gdouble spec_lstart; /* V 3.0: for all spectroscopy modes! */
   gdouble spec_lend;
   gdouble spec_linc;
   guint32 spec_lsteps;
   gdouble spec_rstart;
   gdouble spec_rend;
   gdouble spec_rinc;
   guint32 spec_rsteps;
   gdouble version; /* height offstes used in version 3.0 */
   gdouble free_lend;
   gdouble free_linc;
   guint32 free_lsteps;
   gdouble free_rstart;
   gdouble free_rend;
   gdouble free_rinc;
   guint32 free_rsteps;
   guint32 timer1; /* timers */
   guint32 timer2;
   guint32 timer3;
   guint32 timer4;
   guint32 m_time; /* time of measurement */
   gdouble u_divider; /* divider for gap voltage */
   gint fb_control;
   gint fb_delay;
   gint point_time; /* time per data point (1/2 set, 1/2 measure) */
   gint spec_time; /* acquisition time spectroscopy point */
   gint spec_delay; /* delay for spectroscopy */
   gint fm; /* fastmode screen imaging */
   gint fm_prgmode;
   gint fm_channel1; /* input channel for fm -> */
   gint fm_channel2; /* input channel for fm <- */
   gint fm_wait; /* wait in ms between frames */
   gint fm_frames; /* number of frames */
   gint fm_delay; /* delay time in 10 us between points */
   gint spectr_edit;
   gint fm_speed; /* speed faktor */
   gint fm_reads; /* 2 exponent of ad-reads */
} StmprgControl;

typedef struct {
   gdouble version;
   gint adc_data_l; /* macro adc values scanning left */
   gint adc_data_r; /* right */
   gint first_zp; /* first z-point before and after scan */
   gint last_zp;
   gdouble zdrift; /* drift in Agstrom (fld.sol_z*(last-first)) */
   gint savememory; /* store one or two directions with D-scan */
   gchar date[20]; /* date of measurement */
   gchar comment[50]; /* comment */
   gchar username[20]; /* login name */
   gchar macro_file[50]; /* macro file used, complete name */
   gchar cext_a[40]; /* 4 bytes for sol_ext1 text, 4 for sol_ext2 text */
   gchar cext_b[40];
   gint contscan; /* CCM continuous scan */
   gint spec_loop;
   gint ext_c;
   gdouble fm_zlift; /* lift z while scanning fast mode */
   gint ext_a;
   gdouble vme_release; /* version of vme-program used for scan */
} StmprgOtherControl;

typedef struct {
    StmprgMainfield mainfield;
    StmprgControl control;
    StmprgOtherControl other_control;
} StmprgFile;

static gboolean      module_register     (void);
static gint          stmprg_detect       (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* stmprg_load         (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static gboolean      read_binary_ubedata (gint n,
                                          gdouble *data,
                                          const guchar *buffer,
                                          gint bpp);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron STMPRG data files (tp ta)."),
    "Rok Zitko <rok.zitko@ijs.si>, Yeti <yeti@gwyddion.net>",
    "0.10",
    "Rok Zitko & David Nečas (Yeti)",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("stmprg",
                           N_("Omicron STMPRG files (tp ta)"),
                           (GwyFileDetectFunc)&stmprg_detect,
                           (GwyFileLoadFunc)&stmprg_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
stmprg_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return strstr(fileinfo->name, "tp") ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC_TXT, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static gboolean
read_parameters(const guchar *buffer,
                guint size,
                StmprgFile *stmprgfile,
                GError **error)
{
    const guchar *p = buffer + MAGIC_SIZE;
    StmprgMainfield *mainfield;
    StmprgControl *control;
    StmprgOtherControl *other_control;

    gwy_debug("tp file size is %u, expecting %u", size, PARAM_SIZE);
    if (size < PARAM_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Parameter file is too short."));
        return FALSE;
    }

    /* mainfield */
    mainfield = &stmprgfile->mainfield;
    mainfield->start_x = gwy_get_gfloat_be(&p);
    mainfield->start_y = gwy_get_gfloat_be(&p);
    mainfield->field_x = gwy_get_gfloat_be(&p);
    mainfield->field_y = gwy_get_gfloat_be(&p);
    mainfield->inc_x = gwy_get_gfloat_be(&p);
    mainfield->inc_y = gwy_get_gfloat_be(&p);
    mainfield->points = gwy_get_gint32_be(&p);
    mainfield->lines = gwy_get_gint32_be(&p);
    mainfield->angle = gwy_get_gfloat_be(&p);
    mainfield->sol_x = gwy_get_gfloat_be(&p);
    mainfield->sol_y = gwy_get_gfloat_be(&p);
    mainfield->sol_z = gwy_get_gfloat_be(&p);
    mainfield->sol_ext1 = gwy_get_gfloat_be(&p);
    mainfield->sol_ext2 = gwy_get_gfloat_be(&p);
    mainfield->sol_h = gwy_get_gfloat_be(&p);
    g_assert(p - buffer == MAGIC_SIZE + MAINFIELD_SIZE);
    gwy_debug("start_x = %g", mainfield->start_x);
    gwy_debug("start_y = %g", mainfield->start_y);
    gwy_debug("field_x = %g", mainfield->field_x);
    gwy_debug("field_y = %g", mainfield->field_y);
    gwy_debug("inc_x = %g", mainfield->inc_x);
    gwy_debug("inc_y = %g", mainfield->inc_y);
    gwy_debug("points = %d", mainfield->points);
    gwy_debug("lines = %d", mainfield->lines);
    gwy_debug("angle = %g", mainfield->angle);
    gwy_debug("sol_x = %g", mainfield->sol_x);
    gwy_debug("sol_y = %g", mainfield->sol_y);
    gwy_debug("sol_z = %g", mainfield->sol_z);
    gwy_debug("sol_ext1 = %g", mainfield->sol_ext1);
    gwy_debug("sol_ext2 = %g", mainfield->sol_ext2);
    gwy_debug("sol_h = %g", mainfield->sol_h);

    if (err_DIMENSION(error, mainfield->points)
        || err_DIMENSION(error, mainfield->lines))
        return FALSE;

    /* control */
    control = &stmprgfile->control;
    control->mode = *(p++);
    control->channel1 = *(p++);
    control->channel2 = *(p++);
    control->channel3 = *(p++);
    control->spectr = *(p++);
    control->cfree = *(p++);
    control->type = gwy_get_gint16_be(&p);
    control->steps_x = gwy_get_gint32_be(&p);
    control->steps_y = gwy_get_gint32_be(&p);
    control->dac_speed = gwy_get_gint32_be(&p);
    control->poi_inc = gwy_get_gfloat_be(&p);
    control->lin_inc = gwy_get_gfloat_be(&p);
    control->ad1_reads = gwy_get_gint32_be(&p);
    control->ad2_reads = gwy_get_gint32_be(&p);
    control->ad3_reads = gwy_get_gint32_be(&p);
    control->analog_ave = gwy_get_gint32_be(&p);
    control->speed = gwy_get_gint32_be(&p);
    control->voltage = gwy_get_gfloat_be(&p);
    control->voltage_l = gwy_get_gfloat_be(&p);
    control->voltage_r = gwy_get_gfloat_be(&p);
    control->volt_flag = gwy_get_gint32_be(&p);
    control->volt_region = gwy_get_gint32_be(&p);
    control->current = gwy_get_gfloat_be(&p);
    control->current_l = gwy_get_gfloat_be(&p);
    control->current_r = gwy_get_gfloat_be(&p);
    control->curr_flag = gwy_get_gint32_be(&p);
    control->curr_region = gwy_get_gint32_be(&p);
    control->spec_lstart = gwy_get_gfloat_be(&p);
    control->spec_lend = gwy_get_gfloat_be(&p);
    control->spec_linc = gwy_get_gfloat_be(&p);
    control->spec_lsteps = gwy_get_guint32_be(&p);
    control->spec_rstart = gwy_get_gfloat_be(&p);
    control->spec_rend = gwy_get_gfloat_be(&p);
    control->spec_rinc = gwy_get_gfloat_be(&p);
    control->spec_rsteps = gwy_get_guint32_be(&p);
    control->version = gwy_get_gfloat_be(&p);
    control->free_lend = gwy_get_gfloat_be(&p);
    control->free_linc = gwy_get_gfloat_be(&p);
    control->free_lsteps = gwy_get_guint32_be(&p);
    control->free_rstart = gwy_get_gfloat_be(&p);
    control->free_rend = gwy_get_gfloat_be(&p);
    control->free_rinc = gwy_get_gfloat_be(&p);
    control->free_rsteps = gwy_get_guint32_be(&p);
    control->timer1 = gwy_get_guint32_be(&p);
    control->timer2 = gwy_get_guint32_be(&p);
    control->timer3 = gwy_get_guint32_be(&p);
    control->timer4 = gwy_get_guint32_be(&p);
    control->m_time = gwy_get_guint32_be(&p);
    control->u_divider = gwy_get_gfloat_be(&p);
    control->fb_control = gwy_get_gint32_be(&p);
    control->fb_delay = gwy_get_gint32_be(&p);
    control->point_time = gwy_get_gint32_be(&p);
    control->spec_time = gwy_get_gint32_be(&p);
    control->spec_delay = gwy_get_gint32_be(&p);
    control->fm = gwy_get_gint16_be(&p);
    control->fm_prgmode = gwy_get_gint16_be(&p);
    control->fm_channel1 = gwy_get_gint32_be(&p);
    control->fm_channel2 = gwy_get_gint32_be(&p);
    control->fm_wait = gwy_get_gint32_be(&p);
    control->fm_frames = gwy_get_gint32_be(&p);
    control->fm_delay = gwy_get_gint16_be(&p);
    control->spectr_edit = gwy_get_gint16_be(&p);
    control->fm_speed = gwy_get_gint16_be(&p);
    control->fm_reads = gwy_get_gint16_be(&p);
    g_assert(p - buffer == MAGIC_SIZE + MAINFIELD_SIZE + CONTROL_SIZE);
    gwy_debug("type = %d", control->type);
    gwy_debug("steps_x = %d", control->steps_x);
    gwy_debug("steps_y = %d", control->steps_y);
    gwy_debug("dac_speed = %d", control->dac_speed);
    gwy_debug("poi_inc = %g", control->poi_inc);
    gwy_debug("lin_inc = %g", control->lin_inc);
    gwy_debug("ad1_reads = %d", control->ad1_reads);
    gwy_debug("ad2_reads = %d", control->ad2_reads);
    gwy_debug("ad3_reads = %d", control->ad3_reads);
    gwy_debug("analog_ave = %d", control->analog_ave);
    gwy_debug("speed = %d", control->speed);
    gwy_debug("voltage = %g", control->voltage);
    gwy_debug("voltage_l = %g", control->voltage_l);
    gwy_debug("voltage_r = %g", control->voltage_r);
    gwy_debug("volt_flag = %d", control->volt_flag);
    gwy_debug("volt_region = %d", control->volt_region);
    gwy_debug("current = %g", control->current);
    gwy_debug("current_l = %g", control->current_l);
    gwy_debug("current_r = %g", control->current_r);
    gwy_debug("curr_flag = %d", control->curr_flag);
    gwy_debug("curr_region = %d", control->curr_region);
    gwy_debug("spec_lstart = %g", control->spec_lstart);
    gwy_debug("spec_lend = %g", control->spec_lend);
    gwy_debug("spec_linc = %g", control->spec_linc);
    gwy_debug("spec_lsteps = %u", control->spec_lsteps);
    gwy_debug("spec_rstart = %g", control->spec_rstart);
    gwy_debug("spec_rend = %g", control->spec_rend);
    gwy_debug("spec_rinc = %g", control->spec_rinc);
    gwy_debug("spec_rsteps = %u", control->spec_rsteps);
    gwy_debug("version = %g", control->version);
    gwy_debug("free_lend = %g", control->free_lend);
    gwy_debug("free_linc = %g", control->free_linc);
    gwy_debug("free_lsteps = %u", control->free_lsteps);
    gwy_debug("free_rstart = %g", control->free_rstart);
    gwy_debug("free_rend = %g", control->free_rend);
    gwy_debug("free_rinc = %g", control->free_rinc);
    gwy_debug("free_rsteps = %u", control->free_rsteps);
    gwy_debug("timer1 = %u", control->timer1);
    gwy_debug("timer2 = %u", control->timer2);
    gwy_debug("timer3 = %u", control->timer3);
    gwy_debug("timer4 = %u", control->timer4);
    gwy_debug("m_time = %u", control->m_time);
    gwy_debug("u_divider = %g", control->u_divider);
    gwy_debug("fb_control = %d", control->fb_control);
    gwy_debug("fb_delay = %d", control->fb_delay);
    gwy_debug("point_time = %d", control->point_time);
    gwy_debug("spec_time = %d", control->spec_time);
    gwy_debug("spec_delay = %d", control->spec_delay);
    gwy_debug("fm = %d", control->fm);
    gwy_debug("fm_prgmode = %d", control->fm_prgmode);
    gwy_debug("fm_channel1 = %d", control->fm_channel1);
    gwy_debug("fm_channel2 = %d", control->fm_channel2);
    gwy_debug("fm_wait = %d", control->fm_wait);
    gwy_debug("fm_frames = %d", control->fm_frames);
    gwy_debug("fm_delay = %d", control->fm_delay);
    gwy_debug("spectr_edit = %d", control->spectr_edit);
    gwy_debug("fm_speed = %d", control->fm_speed);
    gwy_debug("fm_reads = %d", control->fm_reads);

    /* other_control */
    other_control = &stmprgfile->other_control;
    other_control->version = gwy_get_gfloat_be(&p);
    other_control->adc_data_l = gwy_get_gint32_be(&p);
    other_control->adc_data_r = gwy_get_gint32_be(&p);
    other_control->first_zp = gwy_get_gint16_be(&p);
    other_control->last_zp = gwy_get_gint16_be(&p);
    other_control->zdrift = gwy_get_gfloat_be(&p);
    other_control->savememory = gwy_get_gint32_be(&p);
    get_CHARARRAY0(other_control->date, &p);
    get_CHARARRAY0(other_control->comment, &p);
    get_CHARARRAY0(other_control->username, &p);
    get_CHARARRAY0(other_control->macro_file, &p);
    get_CHARARRAY0(other_control->cext_a, &p);
    get_CHARARRAY0(other_control->cext_b, &p);
    other_control->contscan = gwy_get_gint32_be(&p);
    other_control->spec_loop = gwy_get_gint32_be(&p);
    other_control->ext_c = gwy_get_gint32_be(&p);
    other_control->fm_zlift = gwy_get_gfloat_be(&p);
    other_control->ext_a = gwy_get_gint32_be(&p);
    other_control->vme_release = gwy_get_gfloat_be(&p);
    g_assert(p - buffer == PARAM_SIZE);
    gwy_debug("version = %g", other_control->version);
    gwy_debug("adc_data_l = %d", other_control->adc_data_l);
    gwy_debug("adc_data_r = %d", other_control->adc_data_r);
    gwy_debug("first_zp = %d", other_control->first_zp);
    gwy_debug("last_zp = %d", other_control->last_zp);
    gwy_debug("zdrift = %g", other_control->zdrift);
    gwy_debug("savememory = %d", other_control->savememory);
    gwy_debug("date = %s", other_control->date);
    gwy_debug("comment = %s", other_control->comment);
    gwy_debug("username = %s", other_control->username);
    gwy_debug("macro_file = %s", other_control->macro_file);
    gwy_debug("cext_a = %s", other_control->cext_a);
    gwy_debug("cext_b = %s", other_control->cext_b);
    gwy_debug("contscan = %d", other_control->contscan);
    gwy_debug("spec_loop = %d", other_control->spec_loop);
    gwy_debug("ext_c = %d", other_control->ext_c);
    gwy_debug("fm_zlift = %g", other_control->fm_zlift);
    gwy_debug("ext_a = %d", other_control->ext_a);
    gwy_debug("vme_release = %g", other_control->vme_release);

    return TRUE;
}

static GwyDataField*
read_datafield(const guchar *buffer,
               guint size,
               const StmprgFile *stmprgfile,
               GError **error)
{
    gint xres, yres, bpp;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    gdouble *data;
    GwySIUnit *unit;

    bpp = 2;                    /* words, always */

    xres = stmprgfile->mainfield.points;
    yres = stmprgfile->mainfield.lines;
    xreal = Angstrom * stmprgfile->mainfield.field_x;
    yreal = Angstrom * stmprgfile->mainfield.field_y;
    q = stmprgfile->mainfield.sol_z * 1.0e-5;       /* 5 ?? */
    /* resolution of z value in angstrom/bit, 1.0e-10 */

    if (err_SIZE_MISMATCH(error, bpp*xres*yres, size, FALSE))
        return NULL;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    if (!read_binary_ubedata(xres * yres, data, buffer, bpp)) {
        err_BPP(error, bpp);
        g_object_unref(dfield);
        return NULL;
    }

    gwy_data_field_multiply(dfield, q);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    /* Assuming we are reading channel1... */
    switch (stmprgfile->control.channel1) {
        case STMPRG_CHANNEL_OFF:
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("First channel is switched off."));
        return NULL;

        case STMPRG_CHANNEL_Z:
        unit = gwy_si_unit_new("m");
        break;

        case STMPRG_CHANNEL_I:
        case STMPRG_CHANNEL_I_I0:
        case STMPRG_CHANNEL_I0:
        unit = gwy_si_unit_new("A");
        break;

        case STMPRG_CHANNEL_EXT1:
        case STMPRG_CHANNEL_EXT2:
        case STMPRG_CHANNEL_U0:
        unit = gwy_si_unit_new("V");
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    return dfield;
}

/* Macros for storing meta data */

#define HASH_STORE(format, keystr, val) \
    value = g_strdup_printf(format, stmprgfile->val); \
    gwy_debug("key = %s, val = %s", keystr, value); \
    gwy_container_set_string_by_name(meta, keystr, value);

#define HASH_STORE_F(keystr, valf) HASH_STORE("%f", keystr, valf)
#define HASH_STORE_S(keystr, vals) HASH_STORE("%s", keystr, vals)
#define HASH_STORE_I(keystr, vali) HASH_STORE("%i", keystr, vali)

static GwyContainer*
stmprg_get_metadata(const StmprgFile *stmprgfile)
{
    GwyContainer *meta;
    gchar *value;

    meta = gwy_container_new();

    HASH_STORE_F("inc_x", mainfield.inc_x);
    HASH_STORE_F("inc_y", mainfield.inc_y);
    HASH_STORE_F("angle", mainfield.angle);
    HASH_STORE_F("sol_z", mainfield.sol_z);
    HASH_STORE_F("voltage", control.voltage);
    HASH_STORE_F("current", control.current);
    HASH_STORE_I("point_time", control.point_time);
    HASH_STORE_S("date", other_control.date);
    HASH_STORE_S("comment", other_control.comment);
    HASH_STORE_S("username", other_control.username);

    return meta;
}

static GwyContainer*
stmprg_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *meta, *container = NULL;
    StmprgFile stmprgfile;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield;
    char *filename_ta, *ptr;
    gboolean ok;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC_TXT, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Omicron STMPRG");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    memset(&stmprgfile, 0, sizeof(StmprgFile));
    ok = read_parameters(buffer, size, &stmprgfile, error);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!ok)
        return NULL;

    filename_ta = g_strdup(filename);
    ptr = filename_ta + strlen(filename_ta) - 1;
    while (g_ascii_isdigit(*ptr) && ptr > filename_ta+1)
        ptr--;

    if (ptr == filename_ta+1) {
        err_DATA_PART(error, filename);
        g_free(filename_ta);
        return NULL;
    }

    /* Accept lowercase and uppercase and try the same case for data file */
    if (*ptr == 'p' && *(ptr - 1) == 't')
        *ptr = 'a';
    if (*ptr == 'P' && *(ptr - 1) == 'T')
        *ptr = 'A';

    if (!gwy_file_get_contents(filename_ta, &buffer, &size, &err)) {
        g_free(filename_ta);
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    g_free(filename_ta);

    dfield = read_datafield(buffer, size, &stmprgfile, error);
    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        /* FIXME: with documentation, we could perhaps do better */
        gwy_app_channel_title_fall_back(container, 0);

        meta = stmprg_get_metadata(&stmprgfile);
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

/*
 * Warning: read_binary_ubedata reads UNSIGNED BIGENDIAN data.
 */
static gboolean
read_binary_ubedata(gint n,
                    gdouble *data,
                    const guchar *buffer,
                    gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8 * bpp));
    switch (bpp) {
        case 1:
            for (i = 0; i < n; i++)
                data[i] = q * buffer[i];
            break;

        case 2: {
            guint16 *p = (guint16 *) buffer;

            for (i = 0; i < n; i++) {
                data[i] = q * GUINT16_FROM_BE(p[i]);
            }
        }
        break;

        case 4: {
            guint32 *p = (guint32 *)buffer;

            for (i = 0; i < n; i++) {
                data[i] = q * GUINT32_FROM_BE(p[i]);
            }
        }
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
