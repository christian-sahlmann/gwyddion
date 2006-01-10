/*
 *  @(#) $Id$
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

struct STMPRG_MAINFIELD
{
   float start_X; /* zeropoint of measurment piece coordinates */
   float start_Y;
   float field_x; /* length of field for scanning in Angstrom */
   float field_y;
   float inc_x; /* increment steps for point and line (x and y) */
   float inc_y; /* in Angstrom */
   gint32 points;
   gint32 lines;
   float angle; /* angle of field in start system */
   float sol_x; /* resolution of x,y,z in Angstrom per Bit */
   float sol_y;
   float sol_z;
   float sol_ext1; /* Ext input 1 */
   float sol_ext2; /* Ext input 2 */
   float sol_h; /* resolution dh for z(i) spectroscopy */
};

#define L_MAINFIELD (sizeof(struct STMPRG_MAINFIELD))

struct STMPRG_CONTROL
{
   char mode; /* on/off bits: 3: Spectr, 4-6: Loop1-3 */
   char channel1; /* measurement channels, 0 = off, see below */
   char channel2;
   char channel3;
   char spectr; /* spectr channel, 0 = off, see below */
   char cfree;
   gint16 type; /* E = 0 or D != 0 - scanning */
   gint32 steps_x; /* number of steps per increment */
   gint32 steps_y;
   gint32 dac_speed; /* slewrate of DAC */
   float poi_inc; /* increment per step in x/y-direction for lines/points */
   float lin_inc; /* in piezo coordinates */
   gint32 ad1_reads; /* Number of ADC 1 average */
   gint32 ad2_reads; /* Number of ADC 2 average */
   gint32 ad3_reads; /* ditto ADC 3 */
   gint32 analog_ave; /* analogous averaging */
   gint32 speed; /* speed for z-scan */
   float voltage; /* normal tunnel voltage */
   float voltage_l; /* voltage for scanning left */
   float voltage_r; /* voltage for scanning right */
   gint32 volt_flag; /* 0 = internal, 1 = remote */
   gint32 volt_region; /* region */
   float current; /* normal tunnel current */
   float current_l; /* current for scanning left */
   float current_r; /* current for scanning right */
   gint32 curr_flag; /* 0 = internal, 1 = remote */
   gint32 curr_region; /* region */
   float spec_lstart; /* V 3.0: for all spectroscopy modes! */
   float spec_lend;
   float spec_linc;
   long spec_lsteps;
   float spec_rstart;
   float spec_rend;
   float spec_rinc;
   long spec_rsteps;
   float version; /* height offstes used in version 3.0 */
   float free_lend;
   float free_linc;
   long free_lsteps;
   float free_rstart;
   float free_rend;
   float free_rinc;
   long free_rsteps;
   long timer1; /* timers */
   long timer2;
   long timer3;
   long timer4;
   long m_time; /* time of measurement */
   float u_divider; /* divider for gap voltage */
   gint32 fb_control;
   gint32 fb_delay;
   gint32 point_time; /* time per data point (1/2 set, 1/2 measure) */
   gint32 spec_time; /* acquisition time spectroscopy point */
   gint32 spec_delay; /* delay for spectroscopy */
   gint16 fm; /* fastmode screen imaging */
   gint16 fm_prgmode;
   gint32 fm_channel1; /* input channel for fm -> */
   gint32 fm_channel2; /* input channel for fm <- */
   gint32 fm_wait; /* wait in ms between frames */
   gint32 fm_frames; /* number of frames */
   gint16 fm_delay; /* delay time in 10 us between points */
   gint16 spectr_edit;
   gint16 fm_speed; /* speed faktor */
   gint16 fm_reads; /* 2 exponent of ad-reads */
};

#define L_CONTROL (sizeof(struct STMPRG_CONTROL))

struct STMPRG_OTHER_CTRL
{
   float version;
   gint32 adc_data_l; /* macro adc values scanning left */
   gint32 adc_data_r; /* right */
   guint16 first_zp; /* first z-point before and after scan */
   guint16 last_zp;
   float zdrift; /* drift in Agstrom (fld.sol_z*(last-first)) */
   gint32 savememory; /* store one or two directions with D-scan */
   char date[20]; /* date of measurement */
   char comment[50]; /* comment */
   char username[20]; /* login name */
   char macro_file[50]; /* macro file used, complete name */
   char cext_a[40]; /* 4 bytes for sol_ext1 text, 4 for sol_ext2 text */
   char cext_b[40];
   gint32 contscan; /* CCM continuous scan */
   gint32 spec_loop;
   gint32 ext_c;
   float fm_zlift; /* lift z while scanning fast mode */
   gint32 ext_a;
   float vme_release; /* version of vme-program used for scan */
};

#define L_OTHER_CTRL (sizeof(struct STMPRG_OTHER_CTRL))

enum {
    STMPRG_CHANNEL_OFF  = 0,
    STMPRG_CHANNEL_Z    = 1,
    STMPRG_CHANNEL_I    = 2,
    STMPRG_CHANNEL_I_I0 = 3,
    STMPRG_CHANNEL_ext1 = 4,
    STMPRG_CHANNEL_ext2 = 5,
    STMPRG_CHANNEL_U0   = 6,
    STMPRG_CHANNEL_I0   = 7
};

enum {
    STMPRG_SPECTRO_CHANNEL_OFF    = 0,
    STMPRG_SPECTRO_CHANNEL_IZ     = 1,
    STMPRG_SPECTRO_CHANNEL_IU0    = 2,
    STMPRG_SPECTRO_CHANNEL_ext1Z  = 3,
    STMPRG_SPECTRO_CHANNEL_ext1U0 = 4,
    STMPRG_SPECTRO_CHANNEL_ext1I0 = 5
};

#define L_SIZE (L_MAINFIELD + L_CONTROL + L_OTHER_CTRL + 4)

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
