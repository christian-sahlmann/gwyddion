/*
 *  $Id$
 *  Copyright (C) 2014 Daniil Bratashov (dn2010).
 *  Data structures and constants are copyright (c) 2011 Renishaw plc.
 *
 *  E-mail: dn2010@gmail.com.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#define DEBUG

 /**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-renishaw-spm">
 *   <comment>Renishaw WiRE Data File</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="WDF1"/>
 *   </magic>
 *   <glob pattern="*.wdf"/>
 *   <glob pattern="*.WDF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Renishaw
 * 0 string WDF1 Renishaw WiRE Data File
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Renishaw WiRE Data File
 * .wdf
 *
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/spectra.h>
#include <libprocess/brick.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "WDF1"
#define MAGIC_SIZE (4)

#define EXTENSION ".wdf"

enum {
    WDF_HEADER_SIZE = 512,
    WDF_BLOCK_HEADER_SIZE = 16
};

typedef enum {
    WDF_BLOCKID_FILE           = 0x31464457UL, /* "WDF1" */
    WDF_BLOCKID_DATA           = 0x41544144UL, /* "DATA" */
    WDF_BLOCKID_YLIST          = 0x54534C59UL, /* "YLST" */
    WDF_BLOCKID_XLIST          = 0x54534C58UL, /* "XLST" */
    WDF_BLOCKID_ORIGIN         = 0x4E47524FUL, /* "ORGN" */
    WDF_BLOCKID_COMMENT        = 0x54584554UL, /* "TEXT" */
    WDF_BLOCKID_WIREDATA       = 0x41445857UL, /* "WXDA" */
    WDF_BLOCKID_DATASETDATA    = 0x42445857UL, /* "WXDB" */
    WDF_BLOCKID_MEASUREMENT    = 0x4D445857UL, /* "WXDM" */
    WDF_BLOCKID_CALIBRATION    = 0x53435857UL, /* "WXCS" */
    WDF_BLOCKID_INSTRUMENT     = 0x53495857UL, /* "WXIS" */
    WDF_BLOCKID_MAPAREA        = 0x50414D57UL, /* "WMAP" */
    WDF_BLOCKID_WHITELIGHT     = 0x4C544857UL, /* "WHTL" */
    WDF_BLOCKID_THUMBNAIL      = 0x4C49414EUL, /* "NAIL" */
    WDF_BLOCKID_MAP            = 0x2050414DUL, /* "MAP " */
    WDF_BLOCKID_CURVEFIT       = 0x52414643UL, /* "CFAR" */
    WDF_BLOCKID_COMPONENT      = 0x534C4344UL, /* "DCLS" */
    WDF_BLOCKID_PCA            = 0x52414350UL, /* "PCAR" */
    WDF_BLOCKID_EM             = 0x4552434DUL, /* "MCRE" */
    WDF_BLOCKID_ZELDAC         = 0x43444C5AUL, /* "ZLDC" */
    WDF_BLOCKID_RESPONSECAL    = 0x4C414352UL, /* "RCAL" */
    WDF_BLOCKID_CAP            = 0x20504143UL, /* "CAP " */
    WDF_BLOCKID_PROCESSING     = 0x50524157UL, /* "WARP" */
    WDF_BLOCKID_ANALYSIS       = 0x41524157UL, /* "WARA" */
    WDF_BLOCKID_SPECTRUMLABELS = 0x4C424C57UL, /* "WLBL" */
    WDF_BLOCKID_CHECKSUM       = 0x4B484357UL, /* "WCHK" */
    WDF_BLOCKID_RXCALDATA      = 0x44435852UL, /* "RXCD" */
    WDF_BLOCKID_RXCALFIT       = 0x46435852UL, /* "RXCF" */
    WDF_BLOCKID_XCAL           = 0x4C414358UL, /* "XCAL" */
    WDF_BLOCKID_SPECSEARCH     = 0x48435253UL, /* "SRCH" */
    WDF_BLOCKID_TEMPPROFILE    = 0x504D4554UL, /* "TEMP" */
    WDF_BLOCKID_UNITCONVERT    = 0x56434E55UL, /* "UNCV" */
    WDF_BLOCKID_ARPLATE        = 0x52505241UL, /* "ARPR" */
    WDF_BLOCKID_ELECSIGN       = 0x43454C45UL, /* "ELEC" */
    WDF_BLOCKID_BKXLIST        = 0x4C584B42UL, /* "BKXL" */
    WDF_BLOCKID_AUXILARYDATA   = 0x20585541UL, /* "AUX " */
    WDF_BLOCKID_CHANGELOG      = 0x474C4843UL, /* "CHLG" */
    WDF_BLOCKID_SURFACE        = 0x46525553UL, /* "SURF" */
    WDF_BLOCKID_ANY            = 0xFFFFFFFFUL,
                           /* reserved value for @ref Wdf_FindSection */
    WDF_STREAM_IS_PSET         = 0x54455350UL  /* "PSET" */
} WdfBlockIDs;

typedef enum {
    WdfDataType_Arbitrary,      /* arbitrary type */
    WdfDataType_Spectral,       /* DEPRECATED: Use Frequency instead */
    WdfDataType_Intensity,      /* intensity */
    WdfDataType_Spatial_X,      /* X position */
    WdfDataType_Spatial_Y,      /* Y axis position */
    WdfDataType_Spatial_Z,      /* Z axis (vertical) position */
    WdfDataType_Spatial_R,      /* rotary stage R axis position */
    WdfDataType_Spatial_Theta,  /* rotary stage theta angle */
    WdfDataType_Spatial_Phi,    /* rotary stage phi angle */
    WdfDataType_Temperature,    /* temperature */
    WdfDataType_Pressure,       /* pressure */
    WdfDataType_Time,           /* time */
    WdfDataType_Derived,        /* derivative type */
    WdfDataType_Polarization,   /* polarization */
    WdfDataType_FocusTrack,     /* focus track Z position */
    WdfDataType_RampRate,       /* temperature ramp rate */
    WdfDataType_Checksum,       /* spectrum data checksum */
    WdfDataType_Flags,          /* bit flags */
    WdfDataType_ElapsedTime,    /* elapsed time intervals */
    WdfDataType_Frequency,      /* frequency */
    /* Microplate mapping origins */
    WdfDataType_Mp_Well_Spatial_X,
    WdfDataType_Mp_Well_Spatial_Y,
    WdfDataType_Mp_LocationIndex,
    WdfDataType_Mp_WellReference,

    WdfDataType_EndMarker       /* THIS SHOULD ALWAYS BE LAST */
} WdfDataType;

typedef enum {
    WdfDataUnits_Arbitrary,    /* arbitrary units */
    WdfDataUnits_RamanShift,   /* Raman shift (cm^-1) */
    WdfDataUnits_Wavenumber,   /* wavenumber (nm) */
    WdfDataUnits_Nanometre,    /* 10-9 metres (nm) */
    WdfDataUnits_ElectronVolt, /* electron volts (eV) */
    WdfDataUnits_Micron,       /* 10-6 metres (um) */
    WdfDataUnits_Counts,       /* counts */
    WdfDataUnits_Electrons,    /* electrons */
    WdfDataUnits_Millimetres,  /* 10^-3 metres (mm) */
    WdfDataUnits_Metres,       /* metres (m) */
    WdfDataUnits_Kelvin,       /* degrees Kelvin (K) */
    WdfDataUnits_Pascal,       /* Pascals (Pa) */
    WdfDataUnits_Seconds,      /* seconds (s) */
    WdfDataUnits_Milliseconds, /* 10^-3 seconds (ms) */
    WdfDataUnits_Hours,
    WdfDataUnits_Days,
    WdfDataUnits_Pixels,
    WdfDataUnits_Intensity,
    WdfDataUnits_RelativeIntensity,
    WdfDataUnits_Degrees,
    WdfDataUnits_Radians,
    WdfDataUnits_Celcius,
    WdfDataUnits_Farenheit,
    WdfDataUnits_KelvinPerMinute,
    WdfDataUnits_FileTime,     /* date-time as a Windows FILETIME */
    WdfDataUnits_EndMarker
} WdfDataUnits;

typedef struct {
    guint32 id;
    guint32 uid;
    guint64 size;
    const guchar *data;
} WdfBlock;

typedef struct {
    guint32 signature;   /* Magic (WDF1) */
    guint32 version;     /* Spec. version */
    guint64 size;        /* The size of this block (512 bytes) */
    guint64 flags;       /* Flags from the WdfFlags */
    guint32 uuid[4];     /* a file unique identifier
                                       - never changed once allocated */
    guint64 unused0;
    guint32 unused1;
    guint32 ntracks;     /* if WdfXYXY flag is set
                                 - contains the number of tracks used */
    guint32 status;      /* file status word (error code) */
    guint32 npoints;     /* number of points per spectrum */
    guint64 nspectra;    /* number of actual spectra (capacity) */
    guint64 ncollected;  /* number of spectra written into the file */
    guint32 naccum;      /* number of accumulations per spectrum */
    guint32 ylistcount;  /* number of elements in the y-list
                                                       (>1 for image) */
    guint32 xlistcount;  /* number of elements for the x-list */
    guint32 origincount; /* number of data origin lists */
    gchar   appname[24]; /* application name (utf-8) */
    guint16 appversion[4]; /* application version
                                         (major, minor, patch, build) */
    guint32 scantype;    /* scan type - WdfScanType */
    guint32 type;        /* measurement type - WdfType */
    guint64 time_start;  /* collection start time as FILETIME */
    guint64 time_end;    /* collection end time as FILETIME */
    guint32 units;       /* spectral data units (one of WdfDataUnits) */
    gfloat  laserwavenum;/* laser wavenumber */
    guint64 spare[6];
    gchar   user[32];    /* utf-8 encoded user name */
    gchar   title[160];  /* utf-8 encoded title */
    guint64 padding[6];  /* padded to 512 bytes */
    guint64 free[4];     /* available for third party use */
    guint64 reserved[4]; /* reserved for internal use by WiRE */
} WdfHeader;

typedef struct {
    WdfHeader *header;
    gfloat    *data;
    gsize      datasize;
} WdfFile;

static gboolean       module_register        (void);
static gint           wdf_detect             (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*  wdf_load               (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gsize          wdf_read_header (const guchar *buffer,
                                       gsize size,
                                       WdfHeader *header,
                                       GError **error);
static gsize          wdf_read_block_header (const guchar *buffer,
                                             gsize size,
                                             WdfBlock *header,
                                             GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Renishaw WiRE data files (WDF)."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.1",
    "Daniil Bratashov (dn2010), Renishaw plc.",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("renishaw",
                           N_("Renishaw WiRE data files (.wdf)"),
                           (GwyFileDetectFunc)&wdf_detect,
                           (GwyFileLoadFunc)&wdf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
wdf_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION))
               ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
wdf_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    gchar *buffer = NULL;
    gsize len, size = 0;
    GError *err = NULL;

    GwyBrick *brick;
    GwyDataField *dfield;
    GwyDataLine *cal;
    gdouble *data;
    WdfFile filedata;
    WdfHeader fileheader;
    WdfBlock block;
    const guchar *p;
    gint i, j, k;
    gint xres = 180, yres =120, zres = 0;
    gdouble xreal = 180*1.4e-6, yreal = 120*1.4e-6, zreal = 1.0;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    p = buffer;
    if ((len = wdf_read_header(p, size, &fileheader, error))
                                                     != WDF_HEADER_SIZE)
        goto fail;
    p += WDF_HEADER_SIZE;
    size -= WDF_HEADER_SIZE;
    filedata.header = &fileheader;
    gwy_debug("npoints = %d, nspectra=%" G_GUINT64_FORMAT "",
              fileheader.npoints,
              fileheader.nspectra);

    while (size > 0) {
        if ((len = wdf_read_block_header(p, size, &block, error)) == 0)
            goto fail;
        if (block.id == WDF_BLOCKID_DATA) {
            gwy_debug("DATA offset = %" G_GUINT64_FORMAT " size=%" G_GUINT64_FORMAT "",
                      ((guint64)p - (guint64)buffer), block.size);
            filedata.data = (gfloat *)p;
            filedata.datasize = block.size - WDF_BLOCK_HEADER_SIZE;
        }
        else if (block.id == WDF_BLOCKID_XLIST) {
            gwy_debug("XLST offset = %" G_GUINT64_FORMAT " size=%" G_GUINT64_FORMAT "",
                      ((guint64)p - (guint64)buffer), block.size);

        }
        else if (block.id == WDF_BLOCKID_ORIGIN) {
            gwy_debug("ORGN offset = %" G_GUINT64_FORMAT " size=%" G_GUINT64_FORMAT "",
                      ((guint64)p - (guint64)buffer), block.size);

        }
        else if (block.id == WDF_BLOCKID_WHITELIGHT) {
            gwy_debug("WL offset = %" G_GUINT64_FORMAT " size=%" G_GUINT64_FORMAT "",
                      ((guint64)p - (guint64)buffer), block.size);

        }

        p += len;
        size -= len;
    }

    zres = fileheader.npoints;
    brick = gwy_brick_new(xres, yres, zres, xreal, yreal, zreal, TRUE);
    data = gwy_brick_get_data(brick);

    /*
    p = buffer + 528;

    for (i = 0; i < xres; i++)
        for (j = 0; j < yres; j++)
            for (k = 0; k < zres; k++) {
                *(data + k * xres * yres + i + j * xres)
                                       = (gdouble)gwy_get_gfloat_le(&p);
            }

    p = buffer + 87696580;

    cal = gwy_data_line_new(zres, zres, FALSE);
    data = gwy_data_line_get_data(cal);
    for (k = 0; k < zres; k++) {
        *(data++) = (gdouble)gwy_get_gfloat_le(&p);;
    }
    // gwy_data_line_set_si_unit_y(cal, mdafile->siunitz);
    gwy_brick_set_zcalibration(brick, cal);
    g_object_unref(cal);

    container = gwy_container_new();

    dfield = gwy_data_field_new(xres, yres,
                                xreal, yreal,
                                TRUE);
    gwy_container_set_object_by_name(container, "/brick/0", brick);
    gwy_container_set_string_by_name(container, "/brick/0/title",
                                     g_strdup("Renishaw"));
    gwy_brick_mean_plane(brick, dfield, 0, 0, 0,
                         xres, yres, -1, FALSE);
    gwy_container_set_object_by_name(container, "/brick/0/preview",
                                     dfield);
    g_object_unref(dfield);
    g_object_unref(brick);

    gwy_file_volume_import_log_add(container, 0, NULL, filename);
    */

    fail:
    g_free(buffer);

    return container;
}

static gsize
wdf_read_header(const guchar *buffer,
                gsize size,
                WdfHeader *header,
                GError **error)
{
    gint i;

    if (size < WDF_HEADER_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        return 0;
    }

    /* identification */
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Renishaw WDF");
        return 0;
    }

    header->signature   = gwy_get_guint32_le(&buffer);
    header->version     = gwy_get_guint32_le(&buffer);
    header->size        = gwy_get_guint64_le(&buffer);
    header->flags       = gwy_get_guint64_le(&buffer);
    for (i = 0; i < 4; i++) {
        header->uuid[i] = gwy_get_guint32_le(&buffer);
    }
    header->unused0     = gwy_get_guint64_le(&buffer);
    header->unused1     = gwy_get_guint32_le(&buffer);
    header->ntracks     = gwy_get_guint32_le(&buffer);
    header->status      = gwy_get_guint32_le(&buffer);
    header->npoints     = gwy_get_guint32_le(&buffer);
    header->nspectra    = gwy_get_guint64_le(&buffer);
    header->ncollected  = gwy_get_guint64_le(&buffer);
    header->naccum      = gwy_get_guint32_le(&buffer);
    header->ylistcount  = gwy_get_guint32_le(&buffer);
    header->xlistcount  = gwy_get_guint32_le(&buffer);
    header->origincount = gwy_get_guint32_le(&buffer);
    for (i = 0; i < 24; i++) {
        header->appname[i] = *(buffer++);
    }
    for (i = 0; i < 4; i++) {
        header->appversion[i] = gwy_get_guint16_le(&buffer);
    }
    header->scantype    = gwy_get_guint32_le(&buffer);
    header->type        = gwy_get_guint32_le(&buffer);
    header->time_start  = gwy_get_guint64_le(&buffer);
    header->time_end    = gwy_get_guint64_le(&buffer);
    header->units       = gwy_get_guint32_le(&buffer);
    header->laserwavenum = gwy_get_gfloat_le(&buffer);
    for (i = 0; i < 6; i++) {
        header->spare[i] = gwy_get_guint64_le(&buffer);
    }
    for (i = 0; i < 24; i++) {
        header->user[i] = *(buffer++);
    }
    for (i = 0; i < 24; i++) {
        header->title[i] = *(buffer++);
    }
    for (i = 0; i < 6; i++) {
        header->padding[i] = gwy_get_guint64_le(&buffer);
    }
    for (i = 0; i < 4; i++) {
        header->free[i] = gwy_get_guint64_le(&buffer);
    }
    for (i = 0; i < 4; i++) {
        header->reserved[i] = gwy_get_guint64_le(&buffer);
    }

    return WDF_HEADER_SIZE;
}

static gsize
wdf_read_block_header(const guchar *buffer,
                      gsize size,
                      WdfBlock *header,
                      GError **error)
{
    if (size < WDF_BLOCK_HEADER_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Block header is truncated"));
        return 0;
    }

    header->id   = gwy_get_guint32_le(&buffer);
    header->uid  = gwy_get_guint32_le(&buffer);
    header->size = gwy_get_guint64_le(&buffer);
    gwy_debug("Block id=%X uid=%d size=%" G_GUINT64_FORMAT "",
              header->id,
              header->uid,
              header->size);

    if (size < header->size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Data block is truncated"));
        return 0;
    }

    header->data = buffer;

    return header->size;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
