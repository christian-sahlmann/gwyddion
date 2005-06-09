/*
 *  Common defines for dealing with JPK files.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
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

#ifndef __JPK_H__
#define __JPK_H__


/*  JPK image scans are TIFF files  */
#define JPK_SCAN_MAGIC      "MM\x00\x2a"
#define JPK_SCAN_MAGIC_SIZE (sizeof(JPK_SCAN_MAGIC) - 1)


/*  Custom TIFF tags  */
#define JPK_TIFFTAG_FileFormatVersion                 0x8000
#define JPK_TIFFTAG_ProgramVersion                    0x8001
#define JPK_TIFFTAG_SavedByProgram                    0x8002
#define JPK_TIFFTAG_StartDate                         0x8003
#define JPK_TIFFTAG_Name                              0x8004
#define JPK_TIFFTAG_Comment                           0x8005
#define JPK_TIFFTAG_EndDate                           0x8006
#define JPK_TIFFTAG_Sample                            0x8007
#define JPK_TIFFTAG_UniqueID                          0x8008
#define JPK_TIFFTAG_AccountName                       0x8009
#define JPK_TIFFTAG_Cantilever_Comment                0x8010
#define JPK_TIFFTAG_Cantilever_SpringConst            0x8011
#define JPK_TIFFTAG_Cantilever_Calibrated             0x8012
#define JPK_TIFFTAG_Cantilever_Shape                  0x8013
#define JPK_TIFFTAG_Cantilever_Radius                 0x8014
#define JPK_TIFFTAG_ApproachID                        0x8015
#define JPK_TIFFTAG_Feedback_Mode                     0x8030
#define JPK_TIFFTAG_Feedback_pGain                    0x8031
#define JPK_TIFFTAG_Feedback_iGain                    0x8032
#define JPK_TIFFTAG_Feedback_Setpoint                 0x8033
#define JPK_TIFFTAG_Feedback_Var1                     0x8034
#define JPK_TIFFTAG_Feedback_Var2                     0x8035
#define JPK_TIFFTAG_Feedback_Var3                     0x8036
#define JPK_TIFFTAG_Feedback_Var4                     0x8037
#define JPK_TIFFTAG_Feedback_Var5                     0x8038
#define JPK_TIFFTAG_Feedback_ApproachAdjustBaseline   JPK_TIFFTAG_Feedback_Var1
#define JPK_TIFFTAG_Feedback_Baseline                 JPK_TIFFTAG_Feedback_Var2
#define JPK_TIFFTAG_Feedback_AdjustReferenceAmplitude JPK_TIFFTAG_Feedback_Var1
#define JPK_TIFFTAG_Feedback_ReferenceAmplitude       JPK_TIFFTAG_Feedback_Var2
#define JPK_TIFFTAG_Feedback_Amplitude                JPK_TIFFTAG_Feedback_Var3
#define JPK_TIFFTAG_Feedback_Frequency                JPK_TIFFTAG_Feedback_Var4
#define JPK_TIFFTAG_Feedback_Phaseshift               JPK_TIFFTAG_Feedback_Var5
#define JPK_TIFFTAG_Approach_IGain                    0x8039
#define JPK_TIFFTAG_Approach_PGain                    0x801A
#define JPK_TIFFTAG_Tipsaver_Setpoint                 0x801B
#define JPK_TIFFTAG_Tipsaver_Active                   0x801C
#define JPK_TIFFTAG_Tipsaver_LowerLimit               0x801D
#define JPK_TIFFTAG_Grid_x0                           0x8040
#define JPK_TIFFTAG_Grid_y0                           0x8041
#define JPK_TIFFTAG_Grid_uLength                      0x8042
#define JPK_TIFFTAG_Grid_vLength                      0x8043
#define JPK_TIFFTAG_Grid_Theta                        0x8044
#define JPK_TIFFTAG_Grid_Reflect                      0x8045
#define JPK_TIFFTAG_Lineend                           0x8048
#define JPK_TIFFTAG_Scanrate_Frequency                0x8049
#define JPK_TIFFTAG_Scanrate_Dutycycle                0x804A
#define JPK_TIFFTAG_Motion                            0x804B
#define JPK_TIFFTAG_Scanline_Start                    0x804C
#define JPK_TIFFTAG_Scanline_Size                     0x804D
#define JPK_TIFFTAG_ForceSettings_Name                0x8050
#define JPK_TIFFTAG_K_Length                          0x8051
#define JPK_TIFFTAG_ForceMap_Feedback_Mode            0x8052
#define JPK_TIFFTAG_Z_Start                           0x8053
#define JPK_TIFFTAG_Z_End                             0x8054
#define JPK_TIFFTAG_Setpoint                          0x8055
#define JPK_TIFFTAG_PauseAtEnd                        0x8056
#define JPK_TIFFTAG_PauseAtStart                      0x8057
#define JPK_TIFFTAG_PauseOnTipsaver                   0x8058
#define JPK_TIFFTAG_TraceScanTime                     0x8059
#define JPK_TIFFTAG_RetraceScanTime                   0x805A
#define JPK_TIFFTAG_Z_Start_Pause_Option              0x805B
#define JPK_TIFFTAG_Z_End_Pause_Option                0x805C
#define JPK_TIFFTAG_Tipsaver_Pause_Option             0x805D
#define JPK_TIFFTAG_Scanner                           0x8060
#define JPK_TIFFTAG_FitAlgorithmName                  0x8061
#define JPK_TIFFTAG_LastIndex                         0x8062
#define JPK_TIFFTAG_BackAndForth                      0x8063

/* Channel tags */
#define JPK_TIFFTAG_Channel                           0x8050
#define JPK_TIFFTAG_Channel_retrace                   0x8051
#define JPK_TIFFTAG_ChannelFancyName                  0x8052
#define JPK_TIFFTAG_Calibration_age                   0x8060
#define JPK_TIFFTAG_Calibration_operator              0x8061

#define JPK_TIFFTAG_NrOfSlots                         0x8080
#define JPK_TIFFTAG_DefaultSlot                       0x8081

#define JPK_TIFFTAG_Slot_Name(n)                     (0x8090 + (n) * 0x30)
#define JPK_TIFFTAG_Slot_Type(n)                     (0x8091 + (n) * 0x30)
#define JPK_TIFFTAG_Slot_Parent(n)                   (0x8092 + (n) * 0x30)
#define JPK_TIFFTAG_Calibration_Name(n)              (0x80A0 + (n) * 0x30)
#define JPK_TIFFTAG_Encoder_Name(n)                  (0x80A1 + (n) * 0x30)
#define JPK_TIFFTAG_Encoder_Unit(n)                  (0x80A2 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Type(n)                  (0x80A3 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Var1(n)                  (0x80A4 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Var2(n)                  (0x80A5 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Var3(n)                  (0x80A6 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Var4(n)                  (0x80A7 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Var5(n)                  (0x80A8 + (n) * 0x30)
#define JPK_TIFFTAG_Scaling_Multiply(n)              JPK_TIFFTAG_Scaling_Var1(n)
#define JPK_TIFFTAG_Scaling_Offset(n)                JPK_TIFFTAG_Scaling_Var2(n)


#endif  /* __JPK_H__ */
