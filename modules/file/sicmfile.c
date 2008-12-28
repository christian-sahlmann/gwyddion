/*
 *  IonScope SICM file format importer
 *  Copyright (C) 2008 Matthew Caldwell.
 *  E-mail: m.caldwell@ucl.ac.uk
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
 * <mime-type type="application/x-sicm-spm">
 *   <comment>IonScope SICM data</comment>
 *   <magic priority="30">
 *     <match type="string" offset="0" value="\x32\x00"/>
 *   </magic>
 * </mime-type>
 **/

/*--------------------------------------------------------------------------
  Dependencies
----------------------------------------------------------------------------*/

#include "config.h"
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

/*--------------------------------------------------------------------------
  Constants
----------------------------------------------------------------------------*/

#define EXTENSION ".img"

enum
{
    SICM_VERSION = 50,
    HEADER_SIZE = 830
};

/*--------------------------------------------------------------------------
  Types
----------------------------------------------------------------------------*/

/* Structure to hold the file header data */
typedef struct _SICMImage
{
                                        /* offset in file */
                                        
    gint16  version; /* 50 */           /* 0 */
    gint16  xdim;                       /* 2 */
    gint16  ydim;                       /* 4 */
    
    /* STM Param Record */
    gdouble fsdHVA;                     /* 6 */
    gdouble fsdDAC;                     /* 12 */
    gdouble fsdADC;                     /* 18 */
    gdouble haGain;                     /* 24 */
    gdouble piezoCalX;                  /* 30 */
    gdouble piezoCalY;                  /* 36 */
    gdouble piezoCalZ;                  /* 42 */
    gdouble gainZ;                      /* 48 */
    gint16  maxADC;                     /* 54 */
    
    /* Scan Para Record */
    gdouble scanSize; /* 10^-8 m */     /* 56 */
    gint16  ctrlOS;                     /* 62 */
    gint16  imagOS;                     /* 64 */
    gint16  ctrlPts;                    /* 66 */
    gint16  xDimension; /* == xdim */   /* 68 */
    gint16  yDimension; /* == ydim */   /* 70 */
    
    /* Loop Record */
    gdouble loopGain;                   /* 72 */
    gdouble setPoint;   /* pA */        /* 78 */
    gdouble tipVoltage; /* mV (?) */    /* 84 */
    gdouble tipXPos;    /* 10^-8 m */   /* 90 */
    gdouble tipYPos;    /* 10^-8 m */   /* 96 */
    
    /* Plane Param Record */
    gdouble A;                          /* 102 */
    gdouble B;                          /* 108 */
    gdouble D;                          /* 114 */
    gint16  fitX;                       /* 120 */
    gint16  fitY;                       /* 122 */
    gint16  min;                        /* 124 */
    gint16  max;                        /* 126 */
    gdouble scale;                      /* 128 */
    
    /* Scan Setup Record */
    gdouble     scanAngle;              /* 134 */
    gdouble     xSlope;                 /* 140 */
    gdouble     ySlope;                 /* 146 */
    gboolean    fitting;                /* 152 */
    gboolean    polarity;               /* 153 */
    gboolean    scan1D;                 /* 154 */
    gboolean    startCenter;            /* 155 */
    
    /* Time Record */
    guchar  date[79];                   /* 156 */
    guchar  time[79];                   /* 235 */
    
    gint16  scanMode;                   /* 314 */
    guint16 version2; /* == version */  /* 316 */
    gdouble range;                      /* 318 */
    guchar  space2[7];                  /* 324 */
    guchar  comment[81];                /* 331 */
    guchar  title[81];                  /* 412 */
    
    /* CITS Record */
    gint16  NCITS;                      /* 493 */
    gint16  settle;                     /* 495 */
    gdouble vArray[8];                  /* 497 */
    gdouble offArray[8];                /* 545 */

    /* Break Record */
    gint16  noPts;                      /* 593 */
    gint16  settle2;                    /* 595 */
    gdouble vStart;                     /* 597 */
    gdouble vEnd;                       /* 603 */
    gdouble threshold;                  /* 609 */
    
    gint16  loopMode;                   /* 615 */
    
    guchar  space[49];                  /* 617 */
    
    /* Info Strings */
    guchar  modeStr[41];                /* 666 */
    guchar  loopStr[41];                /* 707 */
    guchar  sizeStr[41];                /* 748 */
    guchar  posStr[41];                 /* 789 */
    
    /* Start of heightfield data */     /* 830 */
    /* xdim x ydim array of gint16 */
}
SICMImage;

/*--------------------------------------------------------------------------
  Prototypes
----------------------------------------------------------------------------*/

static gboolean module_register ( void );
static gint sicm_detect ( const GwyFileDetectInfo *fileinfo,
                          gboolean only_name );
static GwyContainer* sicm_load ( const gchar *filename,
                                 GwyRunType mode,
                                 GError **error );

/*--------------------------------------------------------------------------
  Implementation
----------------------------------------------------------------------------*/

/* module details */
static GwyModuleInfo module_info =
{
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports IonScope SICM data files."),
    "Matthew Caldwell <m.caldwell@ucl.ac.uk>",
    "1.0",
    "Matthew Caldwell",
    "2007",
};
GWY_MODULE_QUERY(module_info)

/*--------------------------------------------------------------------------*/

/* announce ourselves to gwyddion */
static gboolean module_register ( void )
{
    gwy_file_func_register( "sicm",
                            N_("IonScope SICM files (.img)"),
                            (GwyFileDetectFunc) &sicm_detect,
                            (GwyFileLoadFunc) &sicm_load,
                            NULL,
                            NULL );

    return TRUE;
}

/*--------------------------------------------------------------------------*/

/* check a candidate file for likely SICMness */
static gint sicm_detect ( const GwyFileDetectInfo *fileinfo,
                          gboolean only_name )
{
    /* name isn't much of a test since every second SPM format -- and various
       non-SPM ones too -- use a .IMG extension, but we have nothing else to go on */
    if (only_name)
    {
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;
    }
    else
    {
        /* otherwise, we can perform a slightly more discriminating test: */
        const guchar* p = fileinfo->head;
        
        if ( fileinfo->buffer_len > 6                   /* big enough to test */
             && gwy_get_gint16_le(&p) == SICM_VERSION   /* version has to serve as magic number */
             && (fileinfo->file_size == HEADER_SIZE     /* file size is consistent */
                                        + 2
                                        * gwy_get_gint16_le(&p)
                                        * gwy_get_gint16_le(&p) ) )
            return 100;
    }
    
    return 0;
}

/*--------------------------------------------------------------------------*/

/* load file data. */
static GwyContainer* sicm_load ( const gchar *filename,
                                 G_GNUC_UNUSED GwyRunType mode,
                                 GError **error )
{
    SICMImage sicm;
    GwyContainer *container = NULL, *meta = NULL;
    GwySIUnit *unit;
    GwyDataField *dfield;
    guchar *buffer;
    const guchar *p, *unit_name;
    GError *err = NULL;
    gsize size, expected_size;
    const gint16 *rawdata;
    gdouble *data;
    gdouble scaling;
    gint i,j;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err))
    {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size <= HEADER_SIZE)
    {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer;
    
    sicm.version = gwy_get_gint16_le(&p);
    
    /* version and size tests as in sicm_detect above */
    if ( sicm.version != SICM_VERSION )
    {
        err_FILE_TYPE(error, "SICM");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    
    sicm.xdim = gwy_get_gint16_le(&p);
    sicm.ydim = gwy_get_gint16_le(&p);

    expected_size = 2 * sicm.xdim * sicm.ydim + HEADER_SIZE;
    if (size != expected_size)
    {
        err_SIZE_MISMATCH(error, expected_size, size, TRUE);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* load in the header data */
    sicm.fsdHVA = gwy_get_pascal_real_le(&p);
    sicm.fsdDAC = gwy_get_pascal_real_le(&p);
    sicm.fsdADC = gwy_get_pascal_real_le(&p);
    sicm.haGain = gwy_get_pascal_real_le(&p);
    sicm.piezoCalX = gwy_get_pascal_real_le(&p);
    sicm.piezoCalY = gwy_get_pascal_real_le(&p);
    sicm.piezoCalZ = gwy_get_pascal_real_le(&p);
    sicm.gainZ = gwy_get_pascal_real_le(&p);
    sicm.maxADC = gwy_get_gint16_le(&p);
    
    sicm.scanSize = gwy_get_pascal_real_le(&p);
    sicm.ctrlOS = gwy_get_gint16_le(&p);
    sicm.imagOS = gwy_get_gint16_le(&p);
    sicm.ctrlPts = gwy_get_gint16_le(&p);
    sicm.xDimension = gwy_get_gint16_le(&p);
    sicm.yDimension = gwy_get_gint16_le(&p);
    
    sicm.loopGain = gwy_get_pascal_real_le(&p);
    sicm.setPoint = gwy_get_pascal_real_le(&p);
    sicm.tipVoltage = gwy_get_pascal_real_le(&p);
    sicm.tipXPos = gwy_get_pascal_real_le(&p);
    sicm.tipYPos = gwy_get_pascal_real_le(&p);

    sicm.A = gwy_get_pascal_real_le(&p);
    sicm.B = gwy_get_pascal_real_le(&p);
    sicm.D = gwy_get_pascal_real_le(&p);
    sicm.fitX = gwy_get_gint16_le(&p);
    sicm.fitY = gwy_get_gint16_le(&p);
    sicm.min = gwy_get_gint16_le(&p);
    sicm.max = gwy_get_gint16_le(&p);
    sicm.scale = gwy_get_pascal_real_le(&p);

    sicm.scanAngle = gwy_get_pascal_real_le(&p);
    sicm.xSlope = gwy_get_pascal_real_le(&p);
    sicm.ySlope = gwy_get_pascal_real_le(&p);
    
    sicm.fitting = gwy_get_gboolean8(&p);
    sicm.polarity = gwy_get_gboolean8(&p);
    sicm.scan1D = gwy_get_gboolean8(&p);
    sicm.startCenter = gwy_get_gboolean8(&p);
    
    memcpy(sicm.date, p+1, 78);
    sicm.date[78] = 0;
    p += 79;
    
    memcpy(sicm.time, p+1, 78);
    sicm.time[78] = 0;
    p += 79;
    
    sicm.scanMode = gwy_get_gint16_le(&p);
    sicm.version2 = gwy_get_guint16_le(&p);
    sicm.range = gwy_get_pascal_real_le(&p);
    
    memcpy(sicm.space2, p+1, 6);
    sicm.space2[6] = 0;
    p += 7;
    memcpy(sicm.comment, p+1, 80);
    sicm.comment[80] = 0;
    p += 81;
    memcpy(sicm.title, p+1, 80);
    sicm.title[80] = 0;
    p += 81;
    
    sicm.NCITS = gwy_get_gint16_le(&p);
    sicm.settle = gwy_get_gint16_le(&p);
    for ( i = 0; i < 8; ++i )
        sicm.vArray[i] = gwy_get_pascal_real_le(&p);
    for ( i = 0; i < 8; ++i )
        sicm.offArray[i] = gwy_get_pascal_real_le(&p);
    
    sicm.noPts = gwy_get_gint16_le(&p);
    sicm.settle2 = gwy_get_gint16_le(&p);
    sicm.vStart = gwy_get_pascal_real_le(&p);
    sicm.vEnd = gwy_get_pascal_real_le(&p);
    sicm.threshold = gwy_get_pascal_real_le(&p);

    sicm.loopMode = gwy_get_gint16_le(&p);
    
    memcpy(sicm.space, p+1, 48);
    sicm.space[48] = 0;
    p += 49;
    
    memcpy(sicm.modeStr, p+1, 40);
    sicm.modeStr[40] = 0;
    p += 41;
    memcpy(sicm.loopStr, p+1, 40);
    sicm.loopStr[40] = 0;
    p += 41;
    memcpy(sicm.sizeStr, p+1, 40);
    sicm.sizeStr[40] = 0;
    p += 41;
    memcpy(sicm.posStr, p+1, 40);
    sicm.posStr[40] = 0;
    p += 41;

    dfield = gwy_data_field_new( sicm.xdim,
                                 sicm.ydim,
                                 sicm.scanSize * 1e-8,
                                 sicm.scanSize * 1e-8,
                                 FALSE );
    data = gwy_data_field_get_data ( dfield );
    rawdata = (const gint16*) p;
    
    /* scale factor depends on the channel data type, which we get from the mode string */
    switch ( sicm.modeStr[0] )
    {
        case 'C':
            /* current: we don't have any sensible way of scaling
               this, because the current is measured by an external patch
               amplifier and delivered as a voltage using a mV/pA conversion rate
               that is not recorded in the file. all we can realistically do is
               present that voltage, so fall through to case below */
        
        case 'A':
            /* ADC channels: scale to voltage range */
            scaling = sicm.fsdADC / 32767;
            unit_name = "V";
            break;
        
        default:
            /* topography: scale to piezo range, which we don't explicitly
               know but can determine from sensitivity and voltage range
               with a 1e-6 factor to convert from microns to metres */
            scaling = sicm.piezoCalZ * sicm.fsdHVA * sicm.fsdDAC * 1e-6 / 65534.0;
            unit_name = "m";
    }
    
    
    /* image data is stored bottom-up, so reverse order of rows */
    for (i = 0; i < sicm.ydim; ++i)
    {
        int r1 = i * sicm.xdim;
        int r2 = (sicm.ydim - i - 1) * sicm.xdim;
        
        for ( j = 0; j < sicm.xdim; ++j )
        {
            data[r1 + j] = scaling * GINT16_FROM_LE(rawdata[r2 + j]);
        }
    }
    
    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_new(unit_name);
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    /* build the data container */
    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_convert(sicm.modeStr, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    g_object_unref(dfield);
    
    /* add pretty much all metadata from the header
       a bunch of this doesn't seem to be used meaningfully; we prefix
       probably useless or placeholder fields with a tilde so they sort
       to the bottom of the browser -- these should probably be omitted
       entirely when we're sure they're not needed */
    meta = gwy_container_new();
 
    gwy_container_set_string_by_name(meta,
                                     "Title",
                                     g_convert(sicm.title, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Comment",
                                     g_convert(sicm.comment, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Time Begun",
                                     g_convert(sicm.date, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Time Completed",
                                     g_convert(sicm.time, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Channel Mode",
                                     g_convert(sicm.modeStr, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Loop Info",
                                     g_convert(sicm.loopStr, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Scan Size",
                                     g_convert(sicm.sizeStr, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));
    gwy_container_set_string_by_name(meta,
                                     "Probe Position",
                                     g_convert(sicm.posStr, -1, "UTF-8", "ISO-8859-1", 0, 0, 0 ));

    gwy_container_set_string_by_name(meta,
                                     "Head Voltage Amplifier FSD",
                                     g_strdup_printf("%g V", sicm.fsdHVA));
    gwy_container_set_string_by_name(meta,
                                     "DA Converter FSD",
                                     g_strdup_printf("%g V", sicm.fsdDAC));
    gwy_container_set_string_by_name(meta,
                                     "AD Converter FSD",
                                     g_strdup_printf("%g V", sicm.fsdADC));
    gwy_container_set_string_by_name(meta,
                                     "Head Amplifier Gain",
                                     g_strdup_printf("%g", sicm.haGain));
    gwy_container_set_string_by_name(meta,
                                     "Piezo X Sensitivity",
                                     g_strdup_printf("%g", sicm.piezoCalX));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Y Sensitivity",
                                     g_strdup_printf("%g", sicm.piezoCalY));
    gwy_container_set_string_by_name(meta,
                                     "Piezo Z Sensitivity",
                                     g_strdup_printf("%g", sicm.piezoCalZ));
    gwy_container_set_string_by_name(meta,
                                     "Z Gain",
                                     g_strdup_printf("%g", sicm.gainZ));
    gwy_container_set_string_by_name(meta,
                                     "Size",
                                     g_strdup_printf("%g x10^-8 m", sicm.scanSize));
    gwy_container_set_string_by_name(meta,
                                     "Loop Gain",
                                     g_strdup_printf("%g", sicm.loopGain));
    gwy_container_set_string_by_name(meta,
                                     "Set Point",
                                     g_strdup_printf("%g", sicm.setPoint));
    gwy_container_set_string_by_name(meta,
                                     "~ Tip Voltage",
                                     g_strdup_printf("%g mV", sicm.tipVoltage));
    gwy_container_set_string_by_name(meta,
                                     "Tip Position X",
                                     g_strdup_printf("%g x10^-8 m", sicm.tipXPos));
    gwy_container_set_string_by_name(meta,
                                     "Tip Position Y",
                                     g_strdup_printf("%g x10^-8 m", sicm.tipYPos));
    gwy_container_set_string_by_name(meta,
                                     "~ Plane Param A",
                                     g_strdup_printf("%g", sicm.A));
    gwy_container_set_string_by_name(meta,
                                     "~ Plane Param B",
                                     g_strdup_printf("%g", sicm.B));
    gwy_container_set_string_by_name(meta,
                                     "~ Plane Param D",
                                     g_strdup_printf("%g", sicm.D));
    gwy_container_set_string_by_name(meta,
                                     "~ Plane Param Scale",
                                     g_strdup_printf("%g", sicm.scale));
    gwy_container_set_string_by_name(meta,
                                     "Scan Angle",
                                     g_strdup_printf("%g", sicm.scanAngle));
    gwy_container_set_string_by_name(meta,
                                     "~ Slope X",
                                     g_strdup_printf("%g", sicm.xSlope));
    gwy_container_set_string_by_name(meta,
                                     "~ Slope Y",
                                     g_strdup_printf("%g", sicm.ySlope));
    gwy_container_set_string_by_name(meta,
                                     "~ Range",
                                     g_strdup_printf("%g", sicm.range));
    gwy_container_set_string_by_name(meta,
                                     "~ vArray",
                                     g_strdup_printf("[%g, %g, %g, %g, %g, %g, %g, %g]",
                                     sicm.vArray[0], sicm.vArray[1], sicm.vArray[2], sicm.vArray[3],
                                     sicm.vArray[4], sicm.vArray[4], sicm.vArray[6], sicm.vArray[7]));
    gwy_container_set_string_by_name(meta,
                                     "~ offArray",
                                     g_strdup_printf("[%g, %g, %g, %g, %g, %g, %g, %g]",
                                     sicm.offArray[0], sicm.offArray[1], sicm.offArray[2], sicm.offArray[3],
                                     sicm.offArray[4], sicm.offArray[4], sicm.offArray[6], sicm.offArray[7]));
    gwy_container_set_string_by_name(meta,
                                     "~ V Start",
                                     g_strdup_printf("%g", sicm.vStart));
    gwy_container_set_string_by_name(meta,
                                     "~ V End",
                                     g_strdup_printf("%g", sicm.vEnd));
    gwy_container_set_string_by_name(meta,
                                     "~ Threshold",
                                     g_strdup_printf("%g", sicm.threshold));
                                     
    gwy_container_set_string_by_name(meta,
                                     "ADC Max",
                                     g_strdup_printf("%d", sicm.maxADC));
    gwy_container_set_string_by_name(meta,
                                     "Oversampling (Control)",
                                     g_strdup_printf("%d", sicm.ctrlOS));
    gwy_container_set_string_by_name(meta,
                                     "Oversampling (Image)",
                                     g_strdup_printf("%d", sicm.imagOS));
    gwy_container_set_string_by_name(meta,
                                     "Control Points",
                                     g_strdup_printf("%d", sicm.ctrlPts));
    gwy_container_set_string_by_name(meta,
                                     "~ Fit X",
                                     g_strdup_printf("%d", sicm.fitX));
    gwy_container_set_string_by_name(meta,
                                     "~ Fit Y",
                                     g_strdup_printf("%d", sicm.fitY));
    gwy_container_set_string_by_name(meta,
                                     "~ Min",
                                     g_strdup_printf("%d", sicm.min));
    gwy_container_set_string_by_name(meta,
                                     "~ Max",
                                     g_strdup_printf("%d", sicm.max));
    gwy_container_set_string_by_name(meta,
                                     "~ Scan Mode",
                                     g_strdup_printf("%d", sicm.scanMode));
    gwy_container_set_string_by_name(meta,
                                     "~ NCITS",
                                     g_strdup_printf("%d", sicm.NCITS));
    gwy_container_set_string_by_name(meta,
                                     "~ CITS Settle",
                                     g_strdup_printf("%d", sicm.settle));
    gwy_container_set_string_by_name(meta,
                                     "Break Points",
                                     g_strdup_printf("%d", sicm.noPts));
    gwy_container_set_string_by_name(meta,
                                     "~ Break Settle",
                                     g_strdup_printf("%d", sicm.settle2));
    gwy_container_set_string_by_name(meta,
                                     "Loop Mode",
                                     g_strdup_printf("%d", sicm.loopMode));

    gwy_container_set_string_by_name(meta,
                                     "~ Fitting",
                                     g_strdup(sicm.fitting ? "yes" : "no"));
    gwy_container_set_string_by_name(meta,
                                     "Polarity",
                                     g_strdup(sicm.fitting ? "positive" : "negative"));
    gwy_container_set_string_by_name(meta,
                                     "~ 1D Scan",
                                     g_strdup(sicm.fitting ? "yes" : "no"));
    gwy_container_set_string_by_name(meta,
                                     "Start Center",
                                     g_strdup(sicm.fitting ? "yes" : "no"));
                                     
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    

    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

