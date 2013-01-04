/*
 *  @(#) $Id$
 *  Copyright (C) 2012 Sameer Grover, David Necas (Yeti).
 *  E-mail: sameer.grover.1@gmail.com, yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
 * This file implements a complete implementation Nanomagnetics' NMI file
 * format (version 3) SPM version 1.14.6 (Build 6) has been used to reverse
 * engineer the file format.

 * Suggestions/Comments welcome at sameer.grover.1@gmail.com
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanomagnetics-spm">
 *   <comment>Nanomagnetics AFM data</comment>
 *   <magic priority="100">
 *     <match type="string" offset="0" value="NMI3"/>
 *   </magic>
 *   <glob pattern="*.nmi"/>
 *   <glob pattern="*.NMI"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanomagnetics NMI
 * .nmi
 * Read
 **/

#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".nmi"

#define MAGIC "NMI3"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DCSTEP (10/32768.0)

#define SPMTYPEOPTION 0

/* ENUMS */
typedef enum {
    SPM_RtShpm,
    SPM_LtShpm,
    SPM_RtQtfAfm,
    SPM_LtQtfAfm,
    SPM_LtAfm,
    SPM_NcAfm,
    SPM_LaserAfm,
    SPM_LtMfm,
    SPM_TemNano,
    SPM_Stm,
    SPM_Afm,
    SPM_QShpm,
    SPM_RtQShpm,
    SPM_None
} NMISpmType;

/* XXX XXX XXX: Completely unsafe.  The metadata code assumes that all
 * numerated values are from the list here.  Must be protected! */
static const gchar *SpmTypesStrings[] = {
    "RtShpm",
    "LtShpm",
    "RtQtfAfm",
    "LtQtfAfm",
    "LtAfm",
    "NcAfm",
    "LaserAfm",
    "LtMfm",
    "TemNano",
    "Stm",
    "Afm",
    "QShpm",
    "RtQShpm",
    "None"
};

typedef enum {
    LASER_NonContact,
    LASER_Contact,
    LASER_Ffm,
    LASER_Dynamic,
    LASER_LaserStm,
    LASER_LaserMfm,
    LASER_Efm
} NMILaserMode;

static const gchar *LaserModesStrings[] = {
    "NonContact",
    "Contact",
    "Ffm",
    "Dynamic",
    "LaserStm",
    "LaserMfm",
    "Efm"
};

typedef enum {
    SCAN_NormalScan,
    SCAN_FastScan,
    SCAN_RealTimeScan,
    SCAN_MotorScan,
    SCAN_SpectroscopyScan,
    SCAN_MfmScan,
    SCAN_EfmScan,
    SCAN_Default
} NMIScanType;

static const gchar *ScanTypesStrings[] = {
    "NormalScan",
    "FastScan",
    "RealTimeScan",
    "MotorScan",
    "SpectroscopyScan",
    "MfmScan",
    "EfmScan",
    "Default"
};

typedef enum {
    FEEDBACK_Tunneling,
    FEEDBACK_AfmChannel,
    FEEDBACK_ExternalMode,
    FEEDBACK_ExternalMode2,
    FEEDBACK_ContactMode,
    FEEDBACK_Default
} NMIFeedbackChannelType;

static const gchar *FeedbackChannelTypesStrings[] = {
    "Tunneling",
    "AfmChannel",
    "ExternalMode",
    "ExternalMode2",
    "ContactMode",
    "Default"
};

typedef enum {
    LT_Contact,
    LT_NonContact,
    LT_Dynamic,
    LT_LtStm,
    LT_PrFM,
    LT_SSRM
} NMILtMode;

static const gchar *LtModesStrings[] = {
    "Contact",
    "NonContact",
    "Dynamic",
    "LtStm",
    "PrFM",
    "SSRM"
};

typedef enum {
    CHANNEL_TunnelCurrent,
    CHANNEL_HallVoltage,
    CHANNEL_Vz,
    CHANNEL_Vz4,
    CHANNEL_Ipd,
    CHANNEL_IpdRef,
    CHANNEL_IpdSig,
    CHANNEL_Various,
    CHANNEL_I_HallAdc,
    CHANNEL_V_HallAdc,
    CHANNEL_Spare0,
    CHANNEL_Spare1,
    CHANNEL_Spare2,
    CHANNEL_Spare3,
    CHANNEL_Spare4,
    CHANNEL_Spare5,
    CHANNEL_Spare6,
    CHANNEL_Spare7,
    CHANNEL_Default
} NMIChannel;

static const gchar *ChannelsStrings[] = {
    "TunnelCurrent",
    "HallVoltage",
    "Vz",
    "Vz4",
    "Ipd",
    "IpdRef",
    "IpdSig",
    "Various",
    "I_HallAdc",
    "V_HallAdc",
    "Spare0",
    "Spare1",
    "Spare2",
    "Spare3",
    "Spare4",
    "Spare5",
    "Spare6",
    "Spare7",
    "Default"
};

typedef enum {
    MODE_Afm,
    MODE_Shpm,
    MODE_ConstantCurrentMode,
    MODE_ConstantHeightMode,
    MODE_Spare0,
    MODE_Spare1,
    MODE_Spare2,
    MODE_Spare3,
    MODE_Spare4,
    MODE_Spare5,
    MODE_Spare6,
    MODE_Spare7,
    MODE_Default
} NMIMode;

static const gchar *ModesStrings[] = {
    "Afm",
    "Shpm",
    "ConstantCurrentMode",
    "ConstantHeightMode",
    "Spare0",
    "Spare1",
    "Spare2",
    "Spare3",
    "Spare4",
    "Spare5",
    "Spare6",
    "Spare7",
    "Default"
};

typedef enum {
    UNIT_volt,
    UNIT_ampere,
    UNIT_hertz,
    UNIT_metre,
    UNIT_gauss,
    UNIT_levels,
    UNIT_degree
} NMIUnit;

static const gchar *UnitsStrings[] = {
    "V",
    "A",
    "Hz",
    "m",
    "G",
    "levels",
    "degrees"
};

static const gchar *boolStrings[] = {
    "False",
    "True"
};

/* END ENUMS */

/* PARAMETER STRUCTS */

typedef struct {
    gfloat X, Y;
} NMIVector2D;

typedef struct {
    gfloat X, Y, Z;
} NMIVector3D;

typedef struct {
    gint32 year, month, date, hour, minute, second;
} NMITimeStamp;

typedef struct {
    /* in header */
    gchar *Name;
    gchar *Info;
    guint32 numImages;
    NMISpmType SpmType;
    NMILaserMode LaserMode;
    NMIScanType ScanType;
    NMITimeStamp DateTime;
    gfloat HallProbeOffsetInGauss;
    gfloat kFiber;
    gfloat iHall;
    gfloat rHall;
    gfloat Temperature;
    NMIVector3D HighVoltageAmplifier;
    NMIVector3D PiezoCoefficient;
    NMIVector3D HeatCoefficient;
    NMIVector2D HysAlpha, HysBeta;
    guint32 numAverages;
    gint32 LockIn;
    gfloat DataPointsDelay;
    NMIVector2D CompensationValue;
    NMIVector2D CompensationPercentage;
    gboolean NanoLithoStatus;
    gfloat NanoLithoBiasV;
    gfloat NanoLithoPulseLength;
    gint32 MFM_ScanType;
    gfloat MFM_HeadLiftOff;
    gboolean ForwardScanFeedbackOn;
    gboolean BackwardScanFeedbackOn;
    gboolean QuadratureLockTryCheck;
    NMIFeedbackChannelType FeedbackChannel;
    gfloat vzChannel;
    gboolean hysteresisEnabled;
    gboolean creepCorrectionEnabled;
    NMIVector2D OffsetInstanceIV;
    NMIVector2D OffsetInstanceFD;
    gint32 LaserDriver;
    gfloat FreqQ;
    gfloat FN, FL, FT, GH;
    gfloat ScanSpeed, ScanAngle;
    NMIVector2D ScanOffset;
    NMIVector3D Position;
    gfloat iT, biasV, giV, rSignal, rRef;
    guint16 PhotoGain, DigitalGain, LoopGain;
    gint32 gVirt;
    gfloat Field, FieldRate, FieldApp, FieldMode;
    gfloat PiezoPolinomCoeffs[5];
    gfloat xHysPiezoPolinomCoeffs[5];
    gfloat yHysPiezoPolinomCoeffs[5];
    NMILtMode LtMode;
    gint32 P_value, I_value, D_value, G_value, ScaleXY;
    gfloat CenterFreq, FeedBackValue;
    /* in footer: AFM */
    gfloat DDS2FeedbackPhase;
    gboolean PLL_FeedbackOn;
    gint32 PLL_LockRangeResolution;
    gfloat UserEnteredPLL_CenterFreq;
    gfloat UserEnteredDacValueA, UserEnteredDacValueB;
    gboolean UserEnteredPLL_NegativePolarity;
    gboolean UserEnteredPLL_ConstExc;
    gint32 UserEnteredFeedbackGain;
    gint32 UserEnteredOscAmp;
    gfloat UserEnteredFOffset;
    gfloat UserEnteredRMS;
    gfloat power, f, amp, vpd, vRef, vSignal, fiberVoltage;
    gfloat reflectivity, photoGain, interferenceSlope;
    gfloat oscillationAmp;
    gint64 SSRMGain;
    /* in footer: NCAFM mode */
    gboolean LaserOn, LaserFanOn, LaserRF_State, LockQuad;
    gint32 UserEnteredLaserPower, UserEnteredPhotoGain;
    /* in footer: SHPM mode */
    gboolean HallStatus;
    gfloat HeadLiftOffDist;
    gfloat HeadLiftOffV;
    gfloat UserEnteredHeadLiftoffLateralValue;
    gfloat UserEnteredHallCurrent;
    gfloat UserEnteredHallAmpBW;
    gboolean UserEnteredSwitchInfraRedLed;
    gint32 UserEnteredHallAmpGain;
    gint32 UserEnteredLightIntensity;
    /* Common to AFM and SHPM modes */
    gfloat hallOffset;
    gfloat hallAmpGain;
    gfloat hallAmpBandWidth;
    gfloat headLiftOffVoltage;
    gchar *SoftwareVersion;
    gchar *MotorCardVersion;
    gchar *ScanDacCardVersion;
    gchar *PllVersion;
} NMIParameters;

typedef struct {
    gchar *Name;
    gint32 Width;
    gint32 Height;
    gfloat RealWidth;
    gfloat RealHeight;
    NMIVector2D Volt;
    NMIChannel Channel;
    guint16 Gain;
    gint32 NumAdds;
} NMIChannelParameters;

/* END PARAMETER STRUCTS */


/* FUNCTION DECLARATIONS */
static gboolean      module_register          (void);
static gint          nmi_detect               (const GwyFileDetectInfo *fileinfo,
                                               gboolean only_name);
static GwyContainer* nmi_load                 (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
static gchar*        get_string               (const guchar **p,
                                               gsize *size);
static void          populate_meta_data       (GwyContainer *meta,
                                               NMIParameters *params,
                                               NMIChannelParameters *chparams);
static void          cleanup_global_parameters(NMIParameters *params);
static void          cleanup_channels         (NMIChannelParameters *channels,
                                               gint no_of_channels);
static void          cleanup_data_fields      (GwyDataField **dfields,
                                               gint no_of_channels);

/* functions for getting units */
static NMIUnit get_unit      (NMIParameters *params,
                              NMIChannelParameters *chparams);
static NMIUnit get_unit_base (NMIParameters *params,
                              NMIChannel channel);
static NMIUnit get_unit_AFM  (NMIParameters *params,
                              NMIChannel channel);
static NMIUnit get_unit_NCAFM(NMIParameters *params,
                              NMIChannel channel);
static NMIUnit get_unit_SHPM (NMIParameters *params,
                              NMIChannel channel);

/* functions for calculating true height */
static gboolean calculate_real_height      (gdouble *z,
                                            const NMIParameters *params,
                                            const NMIChannelParameters *chparams);
static gboolean calculate_real_height_base (gint numAdds,
                                            gint16 gain,
                                            gdouble *z,
                                            const NMIParameters *params,
                                            NMIChannel channel);
static gboolean calculate_real_height_AFM  (int numAdds,
                                            guint16 gain,
                                            gdouble *z,
                                            const NMIParameters *params,
                                            NMIChannel channel);
static gboolean calculate_real_height_NCAFM(int numAdds,
                                            guint16 gain,
                                            gdouble *z,
                                            const NMIParameters *params,
                                            NMIChannel channel);
static gboolean calculate_real_height_SHPM (int numAdds,
                                            guint16 gain,
                                            gdouble *z,
                                            const NMIParameters *params,
                                            NMIChannel channel);

/* MODULE BOILERPLATE */

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanomagnetics' NMI file format version 3"),
    "Sameer Grover <sameer.grover.1@gmail.com>",
    "1.0",
    "Sameer Grover, Tata Institute of Fundamental Research",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanomagnetics",
                           _("Nanomagnetics File (.nmi)"),
                           (GwyFileDetectFunc)&nmi_detect,
                           (GwyFileLoadFunc)&nmi_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nmi_detect(const GwyFileDetectInfo * fileinfo, gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

/* END MODULE BOILERPLATE */


/*
   similar to gwy_get_<type>_le with LEB128 encodings as length of string
   prefixed to the actual string arguments: buffer; returns: string (which
   needs to be freed up later)
*/
static gchar*
get_string(const guchar **p, gsize *size)
{
    /* decode LEB128 number (pseudocode from wikipedia) */
    gulong result = 0;
    guint shift = 0;
    guchar byte;
    gchar *strng;
    glong i;

    while (TRUE) {
        if (*size < 1) {
            return NULL;
        }
        byte = (guint) (**p);
        *p += 1;
        *size -= 1;
        result |= ((byte & 0x7F) << shift);
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
    }
    /*allocate memory and read string into it */
    if (*size < result) {
        return NULL;
    }
    strng = g_malloc(result + 1);
    for (i = 0; i < result; i++) {
        strng[i] = (*p)[i];
    }
    strng[i] = 0;
    *p += result;
    *size -= result;
    return strng;
}


/*
  Populate metadata
*/
static void
populate_meta_data(GwyContainer *meta,
                   NMIParameters *params,
                   NMIChannelParameters *chparams)
{
    gwy_container_set_string_by_name(meta, "Name of scan",
                                     g_strdup(params->Name));
    gwy_container_set_string_by_name(meta, "Info",
                                     g_strdup(params->Info));
    gwy_container_set_string_by_name(meta, "Number of Channels",
                                     g_strdup_printf("%u",
                                                     params->numImages));
    gwy_container_set_string_by_name(meta, "SPM Type",
                                     g_strdup(SpmTypesStrings
                                              [params->SpmType]));
    gwy_container_set_string_by_name(meta, "Laser Mode",
                                     g_strdup(LaserModesStrings
                                              [params->LaserMode]));
    gwy_container_set_string_by_name(meta, "Scan Type",
                                     g_strdup(ScanTypesStrings
                                              [params->ScanType]));
    gwy_container_set_string_by_name(meta, "Date and Time",
                                     g_strdup_printf("%u-%u-%u %u:%u:%u",
                                                     params->DateTime.
                                                     year,
                                                     params->DateTime.
                                                     month,
                                                     params->DateTime.
                                                     date,
                                                     params->DateTime.
                                                     hour,
                                                     params->DateTime.
                                                     minute,
                                                     params->DateTime.
                                                     second)
        );
    gwy_container_set_string_by_name(meta, "Hall Probe offset in Gauss",
                                     g_strdup_printf("%f",
                                                     params->HallProbeOffsetInGauss));
    gwy_container_set_string_by_name(meta, "kFiber",
                                     g_strdup_printf("%f",
                                                     params->kFiber));
    gwy_container_set_string_by_name(meta, "iHall",
                                     g_strdup_printf("%f",
                                                     params->iHall));
    gwy_container_set_string_by_name(meta, "rHall",
                                     g_strdup_printf("%f",
                                                     params->rHall));
    gwy_container_set_string_by_name(meta, "Temperature",
                                     g_strdup_printf("%f",
                                                     params->Temperature));
    gwy_container_set_string_by_name(meta, "Piezo High Voltage Amplifier X",
                                     g_strdup_printf("%f",
                                                     params->HighVoltageAmplifier.
                                                     X));
    gwy_container_set_string_by_name(meta, "Piezo High Voltage Amplifier Y",
                                     g_strdup_printf("%f",
                                                     params->HighVoltageAmplifier.
                                                     Y));
    gwy_container_set_string_by_name(meta, "Piezo High Voltage Amplifier Z",
                                     g_strdup_printf("%f",
                                                     params->HighVoltageAmplifier.
                                                     Z));
    gwy_container_set_string_by_name(meta, "Piezo Coefficients X(T)",
                                     g_strdup_printf("%f",
                                                     params->PiezoCoefficient.X));
    gwy_container_set_string_by_name(meta, "Piezo Coefficients Y(T)",
                                     g_strdup_printf("%f",
                                                     params->PiezoCoefficient.Y));
    gwy_container_set_string_by_name(meta, "Piezo Coefficients Z(T)",
                                     g_strdup_printf("%f",
                                                     params->PiezoCoefficient.Z));
    gwy_container_set_string_by_name(meta, "Piezo Heat Coefficient X",
                                     g_strdup_printf("%f",
                                                     params->HeatCoefficient.X));
    gwy_container_set_string_by_name(meta, "Piezo Heat Coefficient Y",
                                     g_strdup_printf("%f",
                                                     params->HeatCoefficient.Y));
    gwy_container_set_string_by_name(meta, "Piezo Heat Coefficient Z",
                                     g_strdup_printf("%f",
                                                     params->HeatCoefficient.Z));
    gwy_container_set_string_by_name(meta, "Piezo Hysterisis Parameters AlphaX",
                                     g_strdup_printf("%f",
                                                     params->HysAlpha.X));
    gwy_container_set_string_by_name(meta, "Piezo Hysterisis Parameters AlphaY",
                                     g_strdup_printf("%f",
                                                     params->HysAlpha.Y));
    gwy_container_set_string_by_name(meta, "Piezo Hysterisis Parameters BetaX",
                                     g_strdup_printf("%f",
                                                     params->HysBeta.X));
    gwy_container_set_string_by_name(meta, "Piezo Hysterisis Parameters BetaY",
                                     g_strdup_printf("%f",
                                                     params->HysBeta.Y));
    gwy_container_set_string_by_name(meta, "Number of averages",
                                     g_strdup_printf("%u",
                                                     params->numAverages));
    gwy_container_set_string_by_name(meta, "LockIn",
                                     g_strdup_printf("%u",
                                                     params->LockIn));
    gwy_container_set_string_by_name(meta, "DataPointsDelay",
                                     g_strdup_printf("%f",
                                                     params->DataPointsDelay));
    gwy_container_set_string_by_name(meta, "Compensation Value X",
                                     g_strdup_printf("%f",
                                                     params->CompensationValue.X));
    gwy_container_set_string_by_name(meta, "Compensation Value Y",
                                     g_strdup_printf("%f",
                                                     params->CompensationValue.Y));
    gwy_container_set_string_by_name(meta, "Compensation Percentage X",
                                     g_strdup_printf("%f",
                                                     params->CompensationPercentage.
                                                     X));
    gwy_container_set_string_by_name(meta, "Compensation Percentage Y",
                                     g_strdup_printf("%f",
                                                     params->CompensationPercentage.
                                                     Y));
    gwy_container_set_string_by_name(meta, "NanoLithoStatus",
                                     g_strdup(boolStrings
                                              [(gint)params->NanoLithoStatus]));
    gwy_container_set_string_by_name(meta, "NanoLithoBiasV",
                                     g_strdup(boolStrings
                                              [(gint)params->NanoLithoBiasV]));
    gwy_container_set_string_by_name(meta, "NanoLithoPulseLength",
                                     g_strdup_printf("%f",
                                                     params->NanoLithoPulseLength));
    gwy_container_set_string_by_name(meta, "MFM ScanType",
                                     g_strdup_printf("%d",
                                                     params->MFM_ScanType));
    gwy_container_set_string_by_name(meta, "MFM HeadLiftOff",
                                     g_strdup_printf("%f",
                                                     params->MFM_HeadLiftOff));
    gwy_container_set_string_by_name(meta, "Forward Scan Feedback On",
                                     g_strdup(boolStrings
                                              [(gint)params->ForwardScanFeedbackOn]));
    gwy_container_set_string_by_name(meta, "Backward Scan Feedback On",
                                     g_strdup(boolStrings
                                              [(gint)params->BackwardScanFeedbackOn]));
    gwy_container_set_string_by_name(meta, "Quadrature Lock Try Check",
                                     g_strdup(boolStrings
                                              [(gint)params->QuadratureLockTryCheck]));
    gwy_container_set_string_by_name(meta, "Feedback Channel",
                                     g_strdup(FeedbackChannelTypesStrings
                                              [params->FeedbackChannel]));
    gwy_container_set_string_by_name(meta, "Vz Channel",
                                     g_strdup_printf("%f",
                                                     params->vzChannel));
    gwy_container_set_string_by_name(meta, "Hysteresis Enabled",
                                     g_strdup(boolStrings
                                              [(gint)params->hysteresisEnabled]));
    gwy_container_set_string_by_name(meta, "Creep Correction Enabled",
                                     g_strdup(boolStrings
                                              [(gint)params->creepCorrectionEnabled]));
    gwy_container_set_string_by_name(meta, "OffsetInstanceIV (X)",
                                     g_strdup_printf("%f",
                                                     params->OffsetInstanceIV.X));
    gwy_container_set_string_by_name(meta, "OffsetInstanceIV (Y)",
                                     g_strdup_printf("%f",
                                                     params->OffsetInstanceIV.Y));
    gwy_container_set_string_by_name(meta, "OffsetInstanceFD (X)",
                                     g_strdup_printf("%f",
                                                     params->OffsetInstanceFD.X));
    gwy_container_set_string_by_name(meta, "OffsetInstanceFD (Y)",
                                     g_strdup_printf("%f",
                                                     params->OffsetInstanceFD.Y));
    gwy_container_set_string_by_name(meta, "Laser Driver",
                                     g_strdup_printf("%d",
                                                     params->LaserDriver));
    gwy_container_set_string_by_name(meta, "FreqQ",
                                     g_strdup_printf("%f",
                                                     params->FreqQ));
    gwy_container_set_string_by_name(meta, "FN",
                                     g_strdup_printf("%f",
                                                     params->FN));
    gwy_container_set_string_by_name(meta, "FL",
                                     g_strdup_printf("%f",
                                                     params->FL));
    gwy_container_set_string_by_name(meta, "FT",
                                     g_strdup_printf("%f",
                                                     params->FT));
    gwy_container_set_string_by_name(meta, "GH",
                                     g_strdup_printf("%f",
                                                     params->GH));
    gwy_container_set_string_by_name(meta, "Scan Speed",
                                     g_strdup_printf("%f",
                                                     params->ScanSpeed));
    gwy_container_set_string_by_name(meta, "ScanAngle",
                                     g_strdup_printf("%f",
                                                     params->ScanAngle));
    gwy_container_set_string_by_name(meta, "Scan Offset X",
                                     g_strdup_printf("%f",
                                                     params->ScanOffset.X));
    gwy_container_set_string_by_name(meta, "Scan Offset Y",
                                     g_strdup_printf("%f",
                                                     params->ScanOffset.Y));
    gwy_container_set_string_by_name(meta, "Position X",
                                     g_strdup_printf("%f",
                                                     params->Position.X));
    gwy_container_set_string_by_name(meta, "Position Y",
                                     g_strdup_printf("%f",
                                                     params->Position.Y));
    gwy_container_set_string_by_name(meta, "Position Z",
                                     g_strdup_printf("%f",
                                                     params->Position.Z));
    gwy_container_set_string_by_name(meta, "iT",
                                     g_strdup_printf("%f",
                                                     params->iT));
    gwy_container_set_string_by_name(meta, "biasV",
                                     g_strdup_printf("%f",
                                                     params->biasV));
    gwy_container_set_string_by_name(meta, "giV",
                                     g_strdup_printf("%f",
                                                     params->giV));
    gwy_container_set_string_by_name(meta, "rSignal",
                                     g_strdup_printf("%f",
                                                     params->rSignal));
    gwy_container_set_string_by_name(meta, "rRef",
                                     g_strdup_printf("%f",
                                                     params->rRef));
    gwy_container_set_string_by_name(meta, "PhotoGain",
                                     g_strdup_printf("%d",
                                                     params->PhotoGain));
    gwy_container_set_string_by_name(meta, "DigitalGain",
                                     g_strdup_printf("%d",
                                                     params->DigitalGain));
    gwy_container_set_string_by_name(meta, "LoopGain",
                                     g_strdup_printf("%d",
                                                     params->LoopGain));
    gwy_container_set_string_by_name(meta, "gVirt",
                                     g_strdup_printf("%d",
                                                     params->gVirt));
    gwy_container_set_string_by_name(meta, "Field",
                                     g_strdup_printf("%f",
                                                     params->Field));
    gwy_container_set_string_by_name(meta, "FieldRate",
                                     g_strdup_printf("%f",
                                                     params->FieldRate));
    gwy_container_set_string_by_name(meta, "FieldApp",
                                     g_strdup_printf("%f",
                                                     params->FieldApp));
    gwy_container_set_string_by_name(meta, "FieldMode",
                                     g_strdup_printf("%f",
                                                     params->FieldMode));
    gwy_container_set_string_by_name(meta, "Piezo Polinom Coefficients a0",
                                     g_strdup_printf("%f",
                                                     params->PiezoPolinomCoeffs[0]));
    gwy_container_set_string_by_name(meta, "Piezo Polinom Coefficients a1",
                                     g_strdup_printf("%f",
                                                     params->PiezoPolinomCoeffs[1]));
    gwy_container_set_string_by_name(meta, "Piezo Polinom Coefficients a2",
                                     g_strdup_printf("%f",
                                                     params->PiezoPolinomCoeffs[2]));
    gwy_container_set_string_by_name(meta, "Piezo Polinom Coefficients a3",
                                     g_strdup_printf("%f",
                                                     params->PiezoPolinomCoeffs[3]));
    gwy_container_set_string_by_name(meta, "Piezo Polinom Coefficients a4",
                                     g_strdup_printf("%f",
                                                     params->PiezoPolinomCoeffs[4]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Xa",
                                     g_strdup_printf("%f",
                                                     params->xHysPiezoPolinomCoeffs
                                                     [0]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Xb",
                                     g_strdup_printf("%f",
                                                     params->xHysPiezoPolinomCoeffs
                                                     [1]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Xc",
                                     g_strdup_printf("%f",
                                                     params->xHysPiezoPolinomCoeffs
                                                     [2]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Xd",
                                     g_strdup_printf("%f",
                                                     params->xHysPiezoPolinomCoeffs
                                                     [3]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Xe",
                                     g_strdup_printf("%f",
                                                     params->xHysPiezoPolinomCoeffs
                                                     [4]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Ya",
                                     g_strdup_printf("%f",
                                                     params->yHysPiezoPolinomCoeffs
                                                     [0]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Yb",
                                     g_strdup_printf("%f",
                                                     params->yHysPiezoPolinomCoeffs
                                                     [1]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Yc",
                                     g_strdup_printf("%f",
                                                     params->yHysPiezoPolinomCoeffs
                                                     [2]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Yd",
                                     g_strdup_printf("%f",
                                                     params->yHysPiezoPolinomCoeffs
                                                     [3]));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Hysterisis Polinom Coefficients Ye",
                                     g_strdup_printf("%f",
                                                     params->yHysPiezoPolinomCoeffs
                                                     [4]));
    gwy_container_set_string_by_name(meta, "LtMode",
                                     g_strdup(LtModesStrings
                                              [params->LtMode]));
    gwy_container_set_string_by_name(meta, "P_value",
                                     g_strdup_printf("%d",
                                                     params->P_value));
    gwy_container_set_string_by_name(meta, "I_value",
                                     g_strdup_printf("%d",
                                                     params->I_value));
    gwy_container_set_string_by_name(meta, "D_value",
                                     g_strdup_printf("%d",
                                                     params->D_value));
    gwy_container_set_string_by_name(meta, "G_value",
                                     g_strdup_printf("%d",
                                                     params->G_value));
    gwy_container_set_string_by_name(meta, "ScaleXY",
                                     g_strdup_printf("%d",
                                                     params->ScaleXY));
    gwy_container_set_string_by_name(meta, "CenterFreq",
                                     g_strdup_printf("%f",
                                                     params->CenterFreq));
    gwy_container_set_string_by_name(meta, "FeedbackValue",
                                     g_strdup_printf("%f",
                                                     params->FeedBackValue));
    if (params->SpmType == SPM_RtShpm
        || params->SpmType == SPM_LtShpm
        || params->SpmType == SPM_TemNano) {
        /* SHPM */
        gwy_container_set_string_by_name(meta, "HallStatus",
                                         g_strdup(boolStrings
                                                  [(gint)params->HallStatus]));
        gwy_container_set_string_by_name(meta, "HeadLiftOffDist",
                                         g_strdup_printf("%f",
                                                         params->HeadLiftOffDist));
        gwy_container_set_string_by_name(meta, "HeadLiftOffV",
                                         g_strdup_printf("%f",
                                                         params->HeadLiftOffV));
        gwy_container_set_string_by_name(meta,
                                         "UserEnteredHeadLiftoffLateralValue",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredHeadLiftoffLateralValue));
        gwy_container_set_string_by_name(meta, "UserEnteredHallCurrent",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredHallCurrent));
        gwy_container_set_string_by_name(meta, "UserEnteredHallAmpBW",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredHallAmpBW));
        gwy_container_set_string_by_name(meta, "UserEnteredSwitchInfraRedLed",
                                         g_strdup(boolStrings
                                                  [(gint)params->UserEnteredSwitchInfraRedLed]));
        gwy_container_set_string_by_name(meta, "UserEnteredHallAmpGain",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredHallAmpGain));
        gwy_container_set_string_by_name(meta, "UserEnteredLightIntensity",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredLightIntensity));
        gwy_container_set_string_by_name(meta, "Hall Offset",
                                         g_strdup_printf("%f",
                                                         params->hallOffset));
        gwy_container_set_string_by_name(meta, "HallAmpGain",
                                         g_strdup_printf("%f",
                                                         params->hallAmpGain));
        gwy_container_set_string_by_name(meta, "HallAmpBandwidth",
                                         g_strdup_printf("%f",
                                                         params->hallAmpBandWidth));
        gwy_container_set_string_by_name(meta, "HeadLiftOffVoltage",
                                         g_strdup_printf("%f",
                                                         params->headLiftOffVoltage));
        gwy_container_set_string_by_name(meta, "Software Version",
                                         g_strdup(params->SoftwareVersion));
        gwy_container_set_string_by_name(meta, "Motor Card Version",
                                         g_strdup(params->MotorCardVersion));
        gwy_container_set_string_by_name(meta, "Scan DAC Card Version",
                                         g_strdup(params->ScanDacCardVersion));
        gwy_container_set_string_by_name(meta, "PLL Version",
                                         g_strdup(params->PllVersion));
    }
    else if (params->SpmType == SPM_NcAfm) {
        /*NCAFM */
        gwy_container_set_string_by_name(meta, "Laser On",
                                         g_strdup(boolStrings
                                                  [params->LaserOn]));
        gwy_container_set_string_by_name(meta, "Laser Fan On",
                                         g_strdup(boolStrings
                                                  [params->LaserFanOn]));
        gwy_container_set_string_by_name(meta, "Laser RF State",
                                         g_strdup(boolStrings
                                                  [params->LaserRF_State]));
        gwy_container_set_string_by_name(meta, "Lock Quad",
                                         g_strdup(boolStrings
                                                  [params->LockQuad]));
        gwy_container_set_string_by_name(meta, "UserEnteredLaserPower",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredLaserPower));
        gwy_container_set_string_by_name(meta, "UserEnteredPhotoGain",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredPhotoGain));
    }
    else {
        /*AFM */
        gwy_container_set_string_by_name(meta, "DDS2FeedbackPhase",
                                         g_strdup_printf("%f",
                                                         params->DDS2FeedbackPhase));
        gwy_container_set_string_by_name(meta, "PLL Feedback On",
                                         g_strdup(boolStrings
                                                  [params->PLL_FeedbackOn]));
        gwy_container_set_string_by_name(meta, "PLL LockRange Resolution",
                                         g_strdup_printf("%d",
                                                         params->PLL_LockRangeResolution));
        gwy_container_set_string_by_name(meta, "UserEntered PLL Center Freq",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredPLL_CenterFreq));
        gwy_container_set_string_by_name(meta, "UserEnteredDacValueA",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredDacValueA));
        gwy_container_set_string_by_name(meta, "UserEnteredDacValueB",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredDacValueB));
        gwy_container_set_string_by_name(meta,
                                         "UserEnteredPLL_NegativePolarity",
                                         g_strdup(boolStrings
                                                  [params->UserEnteredPLL_NegativePolarity]));
        gwy_container_set_string_by_name(meta, "UserEnteredPLL_ConstExc",
                                         g_strdup(boolStrings
                                                  [params->UserEnteredPLL_ConstExc]));
        gwy_container_set_string_by_name(meta, "UserEnteredFeedbackGain",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredFeedbackGain));
        gwy_container_set_string_by_name(meta, "UserEnteredFOffset",
                                         g_strdup_printf("%d",
                                                         params->UserEnteredOscAmp));
        gwy_container_set_string_by_name(meta, "UserEnteredFOffset",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredFOffset));
        gwy_container_set_string_by_name(meta, "UserEnteredRMS",
                                         g_strdup_printf("%f",
                                                         params->UserEnteredRMS));
        gwy_container_set_string_by_name(meta, "power",
                                         g_strdup_printf("%f",
                                                         params->power));
        gwy_container_set_string_by_name(meta, "f",
                                         g_strdup_printf("%f",
                                                         params->f));
        gwy_container_set_string_by_name(meta, "amp",
                                         g_strdup_printf("%f",
                                                         params->amp));
        gwy_container_set_string_by_name(meta, "vpd",
                                         g_strdup_printf("%f",
                                                         params->vpd));
        gwy_container_set_string_by_name(meta, "vRef",
                                         g_strdup_printf("%f",
                                                         params->vRef));
        gwy_container_set_string_by_name(meta, "vSignal",
                                         g_strdup_printf("%f",
                                                         params->vSignal));
        gwy_container_set_string_by_name(meta, "FiberVoltage",
                                         g_strdup_printf("%f",
                                                         params->fiberVoltage));
        gwy_container_set_string_by_name(meta, "Reflectivity",
                                         g_strdup_printf("%f",
                                                         params->reflectivity));
        gwy_container_set_string_by_name(meta, "PhotoGain",
                                         g_strdup_printf("%f",
                                                         params->photoGain));
        gwy_container_set_string_by_name(meta, "Interference Slope",
                                         g_strdup_printf("%f",
                                                         params->interferenceSlope));
        gwy_container_set_string_by_name(meta, "OscillationAmp",
                                         g_strdup_printf("%f",
                                                         params->oscillationAmp));
        /*gwy_container_set_string_by_name(meta, "SSRMGain", g_strdup_printf("%d", params->SSRMGain)); */
        gwy_container_set_string_by_name(meta, "Hall Offset",
                                         g_strdup_printf("%f",
                                                         params->hallOffset));
        gwy_container_set_string_by_name(meta, "HallAmpGain",
                                         g_strdup_printf("%f",
                                                         params->hallAmpGain));
        gwy_container_set_string_by_name(meta, "HallAmpBandwidth",
                                         g_strdup_printf("%f",
                                                         params->hallAmpBandWidth));
        gwy_container_set_string_by_name(meta, "HeadLiftOffVoltage",
                                         g_strdup_printf("%f",
                                                         params->headLiftOffVoltage));
        gwy_container_set_string_by_name(meta, "Software Version",
                                         g_strdup(params->SoftwareVersion));
        gwy_container_set_string_by_name(meta, "Motor Card Version",
                                         g_strdup(params->MotorCardVersion));
        gwy_container_set_string_by_name(meta, "Scan DAC Card Version",
                                         g_strdup(params->ScanDacCardVersion));
        gwy_container_set_string_by_name(meta, "PLL Version",
                                         g_strdup(params->PllVersion));
    }
    gwy_container_set_string_by_name(meta, "Scan Area",
                                     g_strdup_printf("%f X %f",
                                                     chparams->RealWidth,
                                                     chparams-> RealHeight));
    gwy_container_set_string_by_name(meta, "Resolution",
                                     g_strdup_printf("%d X %d",
                                                     chparams->Width,
                                                     chparams->Height));
}

/* Cleanup */

static void
cleanup_global_parameters(NMIParameters *params)
{
    g_free(params->Name);
    g_free(params->Info);
    g_free(params->SoftwareVersion);
    g_free(params->MotorCardVersion);
    g_free(params->ScanDacCardVersion);
    g_free(params->PllVersion);
}

static void
cleanup_channels(NMIChannelParameters *channels, gint no_of_channels)
{
    gint i;

    for (i = 0; i < no_of_channels; i++) {
        g_free(channels[i].Name);
    }
    g_free(channels);
}

static void
cleanup_data_fields(GwyDataField **dfields, gint no_of_channels)
{
    gint i;

    for (i = 0; i < no_of_channels; i++) {
        gwy_object_unref(dfields[i]);
    }
    g_free(dfields);
}


/* THE MAIN FUNCTION */
static GwyContainer *
nmi_load(const gchar *filename, G_GNUC_UNUSED GwyRunType mode, GError **error)
{
    guchar *buffer;
    gsize size, originalsize;
    GError *err = NULL;
    const guchar *p;
    GwyContainer *container, *meta;
    const guint32 *rawdata;
    GwyDataField **dfields;
    GwySIUnit *unit;
    gdouble *data;
    int channelnumber, i;       /*loop counters */
    NMIParameters params;
    NMIChannelParameters *channel_params;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }

    gwy_clear(&params, 1);
    originalsize = size;

    if (size <= MAGIC_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, originalsize, NULL);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Simple");
        gwy_file_abandon_contents(buffer, originalsize, NULL);
        return NULL;
    }

    p = buffer + MAGIC_SIZE;
    size -= MAGIC_SIZE;

    params.Name = get_string(&p, &size);
    if (params.Name == NULL) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, originalsize, NULL);
        cleanup_global_parameters(&params);
        return NULL;
    }

    params.Info = get_string(&p, &size);
    if (params.Info == NULL) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, originalsize, NULL);
        cleanup_global_parameters(&params);
        return NULL;
    }

    if (size < 376) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, originalsize, NULL);
        cleanup_global_parameters(&params);
        return NULL;
    }

    params.numImages = gwy_get_guint32_le(&p);
    params.SpmType = gwy_get_gint32_le(&p) % 14;
    params.LaserMode = gwy_get_gint32_le(&p) % 7;
    params.ScanType = gwy_get_gint32_le(&p) % 8;
    params.DateTime.year = gwy_get_guint32_le(&p);
    params.DateTime.month = gwy_get_guint32_le(&p);
    params.DateTime.date = gwy_get_guint32_le(&p);
    params.DateTime.hour = gwy_get_guint32_le(&p);
    params.DateTime.minute = gwy_get_guint32_le(&p);
    params.DateTime.second = gwy_get_guint32_le(&p);
    params.HallProbeOffsetInGauss = gwy_get_gfloat_le(&p);
    params.kFiber = gwy_get_gfloat_le(&p);
    params.iHall = gwy_get_gfloat_le(&p);
    params.rHall = gwy_get_gfloat_le(&p);
    params.Temperature = gwy_get_gfloat_le(&p);
    params.HighVoltageAmplifier.X = gwy_get_gfloat_le(&p);
    params.HighVoltageAmplifier.Y = gwy_get_gfloat_le(&p);
    params.HighVoltageAmplifier.Z = gwy_get_gfloat_le(&p);
    params.PiezoCoefficient.X = gwy_get_gfloat_le(&p);
    params.PiezoCoefficient.Y = gwy_get_gfloat_le(&p);
    params.PiezoCoefficient.Z = gwy_get_gfloat_le(&p);
    params.HeatCoefficient.X = gwy_get_gfloat_le(&p);
    params.HeatCoefficient.Y = gwy_get_gfloat_le(&p);
    params.HeatCoefficient.Z = gwy_get_gfloat_le(&p);
    params.HysAlpha.X = gwy_get_gfloat_le(&p);
    params.HysAlpha.Y = gwy_get_gfloat_le(&p);
    params.HysBeta.X = gwy_get_gfloat_le(&p);
    params.HysBeta.Y = gwy_get_gfloat_le(&p);
    params.numAverages = gwy_get_guint32_le(&p);
    params.LockIn = gwy_get_gint32_le(&p);
    params.DataPointsDelay = gwy_get_gfloat_le(&p);
    params.CompensationValue.X = gwy_get_gfloat_le(&p);
    params.CompensationValue.Y = gwy_get_gfloat_le(&p);
    params.CompensationPercentage.X = gwy_get_gfloat_le(&p);
    params.CompensationPercentage.Y = gwy_get_gfloat_le(&p);
    params.NanoLithoStatus = gwy_get_gboolean8(&p);
    params.NanoLithoBiasV = gwy_get_gfloat_le(&p);
    params.NanoLithoPulseLength = gwy_get_gfloat_le(&p);
    params.MFM_ScanType = gwy_get_gint32_le(&p);
    params.MFM_HeadLiftOff = gwy_get_gfloat_le(&p);
    params.ForwardScanFeedbackOn = gwy_get_gboolean8(&p);
    params.BackwardScanFeedbackOn = gwy_get_gboolean8(&p);
    params.QuadratureLockTryCheck = gwy_get_gboolean8(&p);
    params.FeedbackChannel = gwy_get_gint32_le(&p) % 6;
    params.vzChannel = gwy_get_gfloat_le(&p);
    params.hysteresisEnabled = gwy_get_gboolean8(&p);
    params.creepCorrectionEnabled = gwy_get_gboolean8(&p);
    params.OffsetInstanceIV.X = gwy_get_gfloat_le(&p);
    params.OffsetInstanceIV.Y = gwy_get_gfloat_le(&p);
    params.OffsetInstanceFD.X = gwy_get_gfloat_le(&p);
    params.OffsetInstanceFD.Y = gwy_get_gfloat_le(&p);
    params.LaserDriver = gwy_get_gint32_le(&p);
    params.FreqQ = gwy_get_gfloat_le(&p);
    params.FN = gwy_get_gfloat_le(&p);
    params.FL = gwy_get_gfloat_le(&p);
    params.FT = gwy_get_gfloat_le(&p);
    params.GH = gwy_get_gfloat_le(&p);
    params.ScanSpeed = gwy_get_gfloat_le(&p);
    params.ScanAngle = gwy_get_gfloat_le(&p);
    params.ScanOffset.X = gwy_get_gfloat_le(&p);;
    params.ScanOffset.Y = gwy_get_gfloat_le(&p);;
    params.Position.X = gwy_get_gfloat_le(&p);;
    params.Position.Y = gwy_get_gfloat_le(&p);;
    params.Position.Z = gwy_get_gfloat_le(&p);;
    params.iT = gwy_get_gfloat_le(&p);
    params.biasV = gwy_get_gfloat_le(&p);
    params.giV = gwy_get_gfloat_le(&p);
    params.rSignal = gwy_get_gfloat_le(&p);
    params.rRef = gwy_get_gfloat_le(&p);
    params.PhotoGain = gwy_get_guint16_le(&p);
    params.DigitalGain = gwy_get_guint16_le(&p);
    params.LoopGain = gwy_get_guint16_le(&p);
    params.gVirt = gwy_get_gint32_le(&p);
    params.Field = gwy_get_gfloat_le(&p);
    params.FieldRate = gwy_get_gfloat_le(&p);
    params.FieldApp = gwy_get_gfloat_le(&p);
    params.FieldMode = gwy_get_gfloat_le(&p);
    params.PiezoPolinomCoeffs[0] = gwy_get_gfloat_le(&p);
    params.PiezoPolinomCoeffs[1] = gwy_get_gfloat_le(&p);
    params.PiezoPolinomCoeffs[2] = gwy_get_gfloat_le(&p);
    params.PiezoPolinomCoeffs[3] = gwy_get_gfloat_le(&p);
    params.PiezoPolinomCoeffs[4] = gwy_get_gfloat_le(&p);
    params.xHysPiezoPolinomCoeffs[0] = gwy_get_gfloat_le(&p);
    params.xHysPiezoPolinomCoeffs[1] = gwy_get_gfloat_le(&p);
    params.xHysPiezoPolinomCoeffs[2] = gwy_get_gfloat_le(&p);
    params.xHysPiezoPolinomCoeffs[3] = gwy_get_gfloat_le(&p);
    params.xHysPiezoPolinomCoeffs[4] = gwy_get_gfloat_le(&p);
    params.yHysPiezoPolinomCoeffs[0] = gwy_get_gfloat_le(&p);
    params.yHysPiezoPolinomCoeffs[1] = gwy_get_gfloat_le(&p);
    params.yHysPiezoPolinomCoeffs[2] = gwy_get_gfloat_le(&p);
    params.yHysPiezoPolinomCoeffs[3] = gwy_get_gfloat_le(&p);
    params.yHysPiezoPolinomCoeffs[4] = gwy_get_gfloat_le(&p);
    params.LtMode = gwy_get_gint32_le(&p) % 6;
    params.P_value = gwy_get_gint32_le(&p);
    params.I_value = gwy_get_gint32_le(&p);
    params.D_value = gwy_get_gint32_le(&p);
    params.G_value = gwy_get_gint32_le(&p);
    params.ScaleXY = gwy_get_gint32_le(&p);
    params.CenterFreq = gwy_get_gfloat_le(&p);
    params.FeedBackValue = gwy_get_gfloat_le(&p);
    size -= 376;

    dfields = g_new0(GwyDataField*, params.numImages);
    channel_params = g_new0(NMIChannelParameters, params.numImages);

    /*Channels start */
    for (channelnumber = 0; channelnumber < params.numImages;
         channelnumber++) {
        NMIChannelParameters *chparams = channel_params + channelnumber;

        chparams->Name = get_string(&p, &size);
        if (chparams->Name == NULL) {
            err_TOO_SHORT(error);
            gwy_file_abandon_contents(buffer, originalsize, NULL);
            cleanup_channels(channel_params, params.numImages);
            cleanup_data_fields(dfields, params.numImages);
            cleanup_global_parameters(&params);
            return NULL;
        }

        if (size < 30) {
            err_TOO_SHORT(error);
            gwy_file_abandon_contents(buffer, originalsize, NULL);
            cleanup_channels(channel_params, params.numImages);
            cleanup_data_fields(dfields, params.numImages);
            cleanup_global_parameters(&params);
            return NULL;
        }

        chparams->Width = gwy_get_gint32_le(&p);
        chparams->Height = gwy_get_gint32_le(&p);
        chparams->RealWidth = gwy_get_gfloat_le(&p);
        chparams->RealHeight = gwy_get_gfloat_le(&p);
        chparams->Volt.X = gwy_get_gfloat_le(&p);
        chparams->Volt.Y = gwy_get_gfloat_le(&p);
        chparams->Channel = gwy_get_gint32_le(&p) % 19;
        chparams->Gain = gwy_get_guint16_le(&p);
        size -= 30;

        /*now read data */
        dfields[channelnumber]
            = gwy_data_field_new(chparams->Width,
                                 chparams->Height,
                                 chparams->RealWidth * 1.0e-10,
                                 chparams->RealHeight * 1.0e-10,
                                 FALSE);
        data = gwy_data_field_get_data(dfields[channelnumber]);

        if (size < chparams->Width * chparams->Height * 4 + 4) {
            err_TOO_SHORT(error);
            gwy_file_abandon_contents(buffer, originalsize, NULL);
            cleanup_channels(channel_params, params.numImages);
            cleanup_data_fields(dfields, params.numImages);
            cleanup_global_parameters(&params);
            return NULL;
        }

        rawdata = (const gint32*)p;
        for (i = 0; i < chparams->Width*chparams->Height; i++) {
            data[i] = GINT32_FROM_LE(rawdata[i]);
        }

        p += (chparams->Width * chparams->Height * 4);
        chparams->NumAdds = gwy_get_gint32_le(&p);
        size -= (chparams->Width * chparams->Height * 4 + 4);
    }

    /*Read Footer: do not raise an error even if footer is not present or is malformed. */
    if (params.SpmType == SPM_RtShpm
        || params.SpmType == SPM_LtShpm
        || params.SpmType == SPM_TemNano) {
        /* SHPM */
        if (size > 1) {
            params.HallStatus = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 4) {
            params.HeadLiftOffDist = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.HeadLiftOffV = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredHeadLiftoffLateralValue
                = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredHallCurrent = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredHallAmpBW = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 1) {
            params.UserEnteredSwitchInfraRedLed
                = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 4) {
            params.UserEnteredHallAmpGain = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredLightIntensity = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallOffset = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallAmpGain = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallAmpBandWidth = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.headLiftOffVoltage = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        params.SoftwareVersion = get_string(&p, &size);
        params.MotorCardVersion = get_string(&p, &size);
        params.ScanDacCardVersion = get_string(&p, &size);
        params.PllVersion = get_string(&p, &size);
    }
    else if (params.SpmType == SPM_NcAfm) {
        /*NCAFM */
        if (size > 1) {
            params.LaserOn = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 1) {
            params.LaserFanOn = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 1) {
            params.LaserRF_State = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 1) {
            params.LockQuad = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 4) {
            params.UserEnteredLaserPower = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredPhotoGain = gwy_get_gint32_le(&p);
            size -= 4;
        }
    }
    else {
        /*AFM */
        if (size > 4) {
            params.DDS2FeedbackPhase = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 1) {
            params.PLL_FeedbackOn = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 4) {
            params.PLL_LockRangeResolution = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredPLL_CenterFreq = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredDacValueA = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredDacValueB = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 1) {
            params.UserEnteredPLL_NegativePolarity
                = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 1) {
            params.UserEnteredPLL_ConstExc = gwy_get_gboolean8(&p);
            size -= 1;
        }
        if (size > 4) {
            params.UserEnteredFeedbackGain = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredOscAmp = gwy_get_gint32_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredFOffset = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.UserEnteredRMS = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.power = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.f = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.amp = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.vpd = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.vRef = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.vSignal = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.fiberVoltage = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.reflectivity = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.photoGain = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.interferenceSlope = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.oscillationAmp = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallOffset = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallAmpGain = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.hallAmpBandWidth = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        if (size > 4) {
            params.headLiftOffVoltage = gwy_get_gfloat_le(&p);
            size -= 4;
        }
        /* if (size>4) { params.SSRMGain = gwy_get_gdouble_le(&p); size -= 4; } Delibrately commented out */
        params.SoftwareVersion = get_string(&p, &size);
        params.MotorCardVersion = get_string(&p, &size);
        params.ScanDacCardVersion = get_string(&p, &size);
        params.PllVersion = get_string(&p, &size);
    }

    /* Read channels all over again, scale the values, set the units and
     * metadata */
    container = gwy_container_new();
    for (channelnumber = 0; channelnumber < params.numImages;
         channelnumber++) {
        NMIChannelParameters *chparams = channel_params + channelnumber;
        /* XXX: calculate_real_height() was originally written to perform the
         * complex procedure to transform each individual data value.  A milion
         * times.  It should be rewritten to just return the scale and offset
         * but that's ugly.  So we get them from it by assuming it's a linear
         * transformation. */
        gdouble offset = 0.0, factor = 1.0;
        if (calculate_real_height(&offset, &params, chparams)
            && calculate_real_height(&factor, &params, chparams))
            factor -= offset;
        else
            factor = offset = 0.0;

        data = gwy_data_field_get_data(dfields[channelnumber]);
        for (i = 0; i < chparams->Width * chparams->Height; i++)
            data[i] = factor*data[i] + offset;

        unit = gwy_si_unit_new("m");
        gwy_data_field_set_si_unit_xy(dfields[channelnumber], unit);
        g_object_unref(unit);

        unit = gwy_si_unit_new(UnitsStrings[get_unit(&params, chparams)]);
        gwy_data_field_set_si_unit_z(dfields[channelnumber], unit);
        g_object_unref(unit);

        gwy_container_set_object_by_name(container,
                                         g_strdup_printf("/%d/data",
                                                         channelnumber),
                                         dfields[channelnumber]);
        gwy_container_set_string_by_name(container,
                                         g_strdup_printf("/%d/data/title",
                                                         channelnumber),
                                         g_strdup(chparams->Name));
        /* set metadata */
        meta = gwy_container_new();
        populate_meta_data(meta, &params, chparams);
        gwy_container_set_object_by_name(container,
                                         g_strdup_printf("/%d/meta",
                                                         channelnumber), meta);
        g_object_unref(meta);
    }

    cleanup_channels(channel_params, params.numImages);
    cleanup_data_fields(dfields, params.numImages);
    cleanup_global_parameters(&params);

    gwy_file_abandon_contents(buffer, originalsize, NULL);

    return container;
}


/*
  Get the units of a particular channel based on
  the channel type and spm scan type
*/
static NMIUnit
get_unit(NMIParameters *params, NMIChannelParameters *chparams)
{
    if (params->SpmType == SPM_RtShpm
        || params->SpmType == SPM_LtShpm
        || params->SpmType == SPM_TemNano) {
        /* SHPM */
        return get_unit_SHPM(params, chparams->Channel);
    }
    else if (params->SpmType == SPM_NcAfm) {
        /*NCAFM */
        return get_unit_NCAFM(params, chparams->Channel);
    }
    else {
        /*AFM */
        return get_unit_AFM(params, chparams->Channel);
    }
}

static gboolean
is_laser_mode(NMISpmType spm_type,
              NMILtMode lt_mode,
              NMILaserMode laser_mode)
{
    if ((spm_type != SPM_LtAfm || lt_mode != LT_Dynamic)
        && spm_type != SPM_LtMfm
        && (spm_type != SPM_LaserAfm || laser_mode != LASER_Dynamic)
        && laser_mode != LASER_LaserMfm) {
        if (spm_type != SPM_LaserAfm)
            return TRUE;
        else
            return laser_mode != LASER_Efm;
    }
    return FALSE;
}

static gboolean
is_hall_channel(NMIChannel channel,
                gboolean also_Vz4,
                gboolean also_HallVoltage)
{
    if (channel == CHANNEL_IpdRef
        || channel == CHANNEL_IpdSig
        || channel == CHANNEL_Various
        || channel == CHANNEL_I_HallAdc
        || channel == CHANNEL_V_HallAdc
        || channel == CHANNEL_Default)
        return TRUE;
    if (also_Vz4 && channel == CHANNEL_Vz4)
        return TRUE;
    if (also_HallVoltage && channel == CHANNEL_HallVoltage)
        return TRUE;
    return FALSE;
}

static gdouble
convert_hall(gdouble val, const NMIParameters *params)
{
    val /= params->iHall * params->rHall * params->GH;
    if (params->GH != 0.001)
        val *= 1e6;
    return val;
}

static gdouble
convert_pll(gdouble val, const NMIParameters *params)
{
    gdouble PLLMasterFrequency = 20000000.0;
    return val * (-PLLMasterFrequency/65535.0
                  * (params->PLL_LockRangeResolution + 1))/20.0;
}

static NMIUnit
get_unit_base(NMIParameters *params, NMIChannel channel)
{
    NMIUnit measure, measure1 = -1;
    gboolean mSpmType, mLtMode;

    if (channel == CHANNEL_TunnelCurrent)
        measure1 = UNIT_ampere;
    else if (channel == CHANNEL_Vz)
        measure1 = UNIT_metre;
    else if (channel == CHANNEL_Spare0)
        measure1 = UNIT_volt;
    else if (channel == CHANNEL_Spare1) {
        if (is_laser_mode(params->SpmType, params->LtMode, params->LaserMode))
            measure1 = UNIT_volt;
        else
            measure1 = UNIT_degree;
    }
    else if (channel == CHANNEL_Spare2) {
        if (params->SpmType != SPM_LtAfm)
            mLtMode = TRUE;
        else
            mLtMode = params->LtMode != LT_SSRM;
        if (mLtMode)
            measure1 = UNIT_volt;
        else
            measure1 = UNIT_ampere;
    }
    else if (channel >= CHANNEL_Spare3 && channel <= CHANNEL_Spare7)
        measure1 = UNIT_volt;
    else {
        measure1 = -1;
        if (params->SpmType != SPM_None)
            measure = measure1;
        else {
            measure1 = UNIT_gauss;
            measure = measure1;
        }
        return measure;
    }
    if (measure1 != -1)
        mSpmType = TRUE;
    else
        mSpmType = params->SpmType != SPM_None;
    if (mSpmType)
        measure = measure1;
    else {
        measure1 = UNIT_gauss;
        measure = measure1;
    }
    return measure;
}

/*AFM UNITS */
static NMIUnit
get_unit_AFM(NMIParameters *params, NMIChannel channel)
{
    NMIUnit measure;
    gboolean ltMode, laserMode, flag, ret_measure1 = FALSE;
    NMIUnit measure1 = get_unit_base(params, channel);

    if (measure1 != -1)
        return measure1;
    if (channel != CHANNEL_HallVoltage) {
        if (channel == CHANNEL_Spare1) {
            if (params->SpmType != SPM_LtQtfAfm)
                measure1 = UNIT_gauss;
            else
                measure1 = UNIT_hertz;
            ret_measure1 = TRUE;
        }
        else {
            if (channel == CHANNEL_Vz4 || channel == CHANNEL_IpdRef
                || channel == CHANNEL_IpdSig || channel == CHANNEL_Various
                || channel == CHANNEL_I_HallAdc
                || channel == CHANNEL_V_HallAdc || channel == CHANNEL_Default)
                ret_measure1 = FALSE;
            else {
                measure1 = UNIT_levels;
                ret_measure1 = TRUE;
            }
        }
    }

    if (ret_measure1) {
        measure = measure1;
        return measure;
    }

    if (params->SpmType != SPM_LtAfm)
        ltMode = TRUE;
    else
        ltMode = params->LtMode != LT_Dynamic;
    if (ltMode) {
        if (params->SpmType != SPM_LaserAfm)
            laserMode = FALSE;
        else {
            if (params->SpmType != SPM_LaserAfm)
                laserMode = TRUE;
            else {
                if (params->LaserMode == LASER_NonContact)
                    laserMode = FALSE;
                else
                    laserMode = params->LaserMode != LASER_Ffm;
            }
        }
        if (laserMode) {
            if (params->SpmType != SPM_LaserAfm)
                flag = TRUE;
            else
                flag = params->LaserMode != LASER_LaserStm;
            if (flag)
                measure1 = UNIT_volt;
            else
                measure1 = UNIT_metre;
        }
        else
            measure1 = UNIT_hertz;
    }
    else
        measure1 = UNIT_volt;

    measure = measure1;
    return measure;
}

/* NCAFM */
static NMIUnit
get_unit_NCAFM(NMIParameters *params, NMIChannel channel)
{
    NMIUnit measure;
    NMIUnit measure1 = get_unit_base(params, channel);

    if (measure1 != -1)
        return measure1;
    if (channel == CHANNEL_HallVoltage || channel == CHANNEL_Vz4
        || channel == CHANNEL_IpdRef || channel == CHANNEL_IpdSig
        || channel == CHANNEL_Various || channel == CHANNEL_I_HallAdc
        || channel == CHANNEL_V_HallAdc || channel == CHANNEL_Default)
        measure1 = UNIT_volt;
    else
        measure1 = UNIT_levels;
    measure = measure1;
    return measure;
}


/*SHPM */
static NMIUnit
get_unit_SHPM(NMIParameters *params, NMIChannel channel)
{
    NMIUnit measure = get_unit_base(params, channel);

    if (measure != -1)
        return measure;

    if (is_hall_channel(channel, FALSE, TRUE))
        return UNIT_gauss;
    else
        return UNIT_levels;
}

/*end units */


/* CALCULATE REAL HEIGHT */

static gboolean
calculate_real_height(gdouble *z,
                      const NMIParameters *params,
                      const NMIChannelParameters *chparams)
{
    gint32 num_adds = chparams->NumAdds, gain = chparams->Gain;
    NMIChannel channel = chparams->Channel;

    if (calculate_real_height_base(num_adds, gain, z, params, channel))
        return TRUE;

    if (params->SpmType == SPM_RtShpm
        || params->SpmType == SPM_LtShpm
        || params->SpmType == SPM_TemNano)
        return calculate_real_height_SHPM(num_adds, gain, z, params, channel);

    if (params->SpmType == SPM_NcAfm)
        return calculate_real_height_NCAFM(num_adds, gain, z, params, channel);

    return calculate_real_height_AFM(num_adds, gain, z, params, channel);
}

static gboolean
calculate_real_height_base(gint numAdds, gint16 gain, gdouble *z,
                           const NMIParameters *params,
                           const NMIChannel channel)
{
    gdouble num = *z - 32768.0*numAdds;
    gdouble val = num * DCSTEP/gain;

    gwy_debug("%u", channel);
    if (channel == CHANNEL_TunnelCurrent)
        *z = 1e-9*DCSTEP * (num - 32768)/0.1;
    else if (channel == CHANNEL_Vz)
        *z = 1e-10*val * (params->HighVoltageAmplifier.Z
                          * params->gVirt * params->PiezoCoefficient.Z);
    else if (channel == CHANNEL_Spare0)
        *z = val;
    else if (channel == CHANNEL_Spare1) {
        if (is_laser_mode(params->SpmType, params->LtMode, params->LaserMode))
            *z = val;
        else {
            *z = (val + 10.0)/20.0;
            *z = *z*360.0 - 180.0;
        }
    }
    else if (channel == CHANNEL_Spare2) {
        *z = val;
        if (params->SpmType == SPM_LtAfm && params->LtMode == LT_SSRM)
            *z /= params->SSRMGain;
    }
    else if (channel >= CHANNEL_Spare3 && channel <= CHANNEL_Spare7)
        *z = val;
    else {
        if (params->SpmType != SPM_None)
            return FALSE;

        *z = convert_hall(val, params);
        return TRUE;
    }

    *z /= numAdds;
    return TRUE;
}

static gboolean
calculate_real_height_AFM(int numAdds, guint16 gain, gdouble *z,
                          const NMIParameters *params,
                          NMIChannel channel)
{
    gdouble num = *z - 32768.0*numAdds;
    gdouble val = num * DCSTEP/gain;

    gwy_debug("%u, %g %g", channel, num, val);
    if (channel != CHANNEL_HallVoltage) {
        if (channel == CHANNEL_Spare1) {
            if (params->SpmType != SPM_LtQtfAfm)
                *z = convert_hall(val, params);
            else
                *z = convert_pll(val, params);
            *z /= numAdds;
            return TRUE;
        }
        else {
            if (!is_hall_channel(channel, TRUE, FALSE)) {
                *z = num/numAdds;
                return TRUE;
            }
        }
    }

    if (params->SpmType != SPM_LtAfm || params->LtMode != LT_Dynamic) {
        if (params->SpmType == SPM_LaserAfm
            && params->LaserMode != LASER_NonContact
            && params->LaserMode != LASER_Ffm) {
            if (params->SpmType != SPM_LaserAfm
                || params->LaserMode != LASER_LaserStm)
                *z = val;
            else
                *z = val * (params->HighVoltageAmplifier.Z
                            * params->gVirt * params->HeatCoefficient.Z);
        }
        else
            *z = convert_pll(val, params);
    }
    else
        *z = val;

    *z /= numAdds;

    return TRUE;
}

static gboolean
calculate_real_height_NCAFM(int numAdds, guint16 gain, gdouble *z,
                            const NMIParameters *params,
                            NMIChannel channel)
{
    gdouble num = *z - 32768.0*numAdds;
    gdouble val = num * DCSTEP/gain;

    gwy_debug("%u", channel);
    if (is_hall_channel(channel, TRUE, TRUE))
        *z = val;
    else
        *z = num;
    *z /= numAdds;
    return TRUE;
}

static gboolean
calculate_real_height_SHPM(int numAdds, guint16 gain, gdouble *z,
                           const NMIParameters *params,
                           NMIChannel channel)
{
    gdouble num = *z - 32768.0*numAdds;
    gdouble val = num * DCSTEP/gain;

    gwy_debug("%u", channel);
    if (is_hall_channel(channel, TRUE, TRUE))
        *z = convert_hall(val, params);
    else
        *z = num;
    *z /= numAdds;
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
