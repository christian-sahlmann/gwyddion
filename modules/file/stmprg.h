/* $Id$
 * Copyright (C) 2004 Rok Zitko
 * E-mail: rok.zitko@ijs.si
 *
 * Information on the format was published in the manual
 * for STMPRG, Copyright (C) 1989-1992 Omicron.
 */

#define INT16 short
#define UINT16 unsigned short
#define INT32 int
#define UINT32 unsigned int

struct STMPRG_MAINFIELD
{
   float start_X; /* zeropoint of measurment piece coordinates */
   float start_Y;
   float field_x; /* length of field for scanning in Angstrom */
   float field_y;
   float inc_x; /* increment steps for point and line (x and y) */
   float inc_y; /* in Angstrom */
   INT32 points;
   INT32 lines;
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
   INT16 type; /* E = 0 or D != 0 - scanning */
   INT32 steps_x; /* number of steps per increment */
   INT32 steps_y;
   INT32 dac_speed; /* slewrate of DAC */
   float poi_inc; /* increment per step in x/y-direction for lines/points */
   float lin_inc; /* in piezo coordinates */
   INT32 ad1_reads; /* Number of ADC 1 average */
   INT32 ad2_reads; /* Number of ADC 2 average */
   INT32 ad3_reads; /* ditto ADC 3 */
   INT32 analog_ave; /* analogous averaging */
   INT32 speed; /* speed for z-scan */
   float voltage; /* normal tunnel voltage */
   float voltage_l; /* voltage for scanning left */
   float voltage_r; /* voltage for scanning right */
   INT32 volt_flag; /* 0 = internal, 1 = remote */
   INT32 volt_region; /* region */
   float current; /* normal tunnel current */
   float current_l; /* current for scanning left */
   float current_r; /* current for scanning right */
   INT32 curr_flag; /* 0 = internal, 1 = remote */
   INT32 curr_region; /* region */
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
   INT32 fb_control;
   INT32 fb_delay;
   INT32 point_time; /* time per data point (1/2 set, 1/2 measure) */
   INT32 spec_time; /* acquisition time spectroscopy point */
   INT32 spec_delay; /* delay for spectroscopy */
   INT16 fm; /* fastmode screen imaging */
   INT16 fm_prgmode;
   INT32 fm_channel1; /* input channel for fm -> */
   INT32 fm_channel2; /* input channel for fm <- */
   INT32 fm_wait; /* wait in ms between frames */
   INT32 fm_frames; /* number of frames */
   INT16 fm_delay; /* delay time in 10 us between points */
   INT16 spectr_edit;
   INT16 fm_speed; /* speed faktor */
   INT16 fm_reads; /* 2 exponent of ad-reads */
};

#define L_CONTROL (sizeof(struct STMPRG_CONTROL))

struct STMPRG_OTHER_CTRL
{
   float version;
   INT32 adc_data_l; /* macro adc values scanning left */
   INT32 adc_data_r; /* right */
   UINT16 first_zp; /* first z-point before and after scan */
   UINT16 last_zp;
   float zdrift; /* drift in Agstrom (fld.sol_z*(last-first)) */
   INT32 savememory; /* store one or two directions with D-scan */
   char date[20]; /* date of measurement */
   char comment[50]; /* comment */
   char username[20]; /* login name */
   char macro_file[50]; /* macro file used, complete name */
   char cext_a[40]; /* 4 bytes for sol_ext1 text, 4 for sol_ext2 text */
   char cext_b[40];
   INT32 contscan; /* CCM continuous scan */
   INT32 spec_loop;
   INT32 ext_c;
   float fm_zlift; /* lift z while scanning fast mode */
   INT32 ext_a;
   float vme_release; /* version of vme-program used for scan */
};

#define L_OTHER_CTRL (sizeof(struct STMPRG_OTHER_CTRL))

#define STMPRG_CHANNEL_OFF  0
#define STMPRG_CHANNEL_Z    1
#define STMPRG_CHANNEL_I    2
#define STMPRG_CHANNEL_I_I0 3
#define STMPRG_CHANNEL_ext1 4
#define STMPRG_CHANNEL_ext2 5
#define STMPRG_CHANNEL_U0   6
#define STMPRG_CHANNEL_I0   7

#define STMPRG_SPECTRO_CHANNEL_OFF    0
#define STMPRG_SPECTRO_CHANNEL_IZ     1
#define STMPRG_SPECTRO_CHANNEL_IU0    2
#define STMPRG_SPECTRO_CHANNEL_ext1Z  3
#define STMPRG_SPECTRO_CHANNEL_ext1U0 4
#define STMPRG_SPECTRO_CHANNEL_ext1I0 5

#define L_SIZE (L_MAINFIELD + L_CONTROL + L_OTHER_CTRL + 4)
