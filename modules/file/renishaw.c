/*
 *  $Id$
 *  Copyright (C) 2014 Daniil Bratashov (dn2010), David Necas (Yeti)..
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
 * Read SPS Volume
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "err.h"
#include "get.h"

#define MAGIC "WDF1"
#define MAGIC_SIZE (4)

#define EXTENSION ".wdf"

enum {
    WDF_HEADER_SIZE = 512,
    WDF_BLOCK_HEADER_SIZE = 16,
    WDF_MAP_AREA_SIZE = 64
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

typedef enum {
    WdfMapArea_RandomPoints = (1 << 0),     /* rectangle area */
    WdfMapArea_ColumnMajor  = (1 << 1),     /* X first then Y. */
    WdfMapArea_Alternating  = (1 << 2),     /* raster or snake */
    WdfMapArea_LineFocusMapping = (1 << 3), /* see also linefocus_height */
    // The following two values are deprecated; negative step-size is sufficient information.
    // [Deprecated] WdfMapArea_InvertedRows = (1 << 4),     /* true if rows collected right to left */
    // [Deprecated] WdfMapArea_InvertedColumns = (1 << 5),  /* true if columns collected bottom to top */
    WdfMapArea_SurfaceProfile = (1 << 6),   /* true if the Z data is non-regular (surface maps) */
    WdfMapArea_XyLine = (1 << 7),           /* line or depth slice forming a single line along the XY plane:
                                               length.x contains number of points along line; length.y = 1 */
} WdfMapAreaType;

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
    guint32 flags;          /* scan mode flags of (WdfMapAreaType)*/
    guint32 unused;
    gfloat  location[3];    /* origin location XYZ */
    gfloat  stepsize[3];    /* real step in XYZ */
    guint32 length[3];      /* xres, yres, zres */
    guint32 linefocus_size; /* length of linefocus line */
} WdfMapArea;

typedef struct {
    WdfHeader    *header;
    gfloat       *data;
    gsize        datasize;
    WdfDataType  xlisttype;
    WdfDataUnits xlistunits;
    gfloat       *xlistdata;
    GdkPixbuf    *whitelight;
    WdfMapArea   *maparea;
} WdfFile;

static gboolean       module_register        (void);
static gint           wdf_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer*  wdf_load               (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gsize          wdf_read_header        (const guchar *buffer,
                                              gsize size,
                                              WdfHeader *header,
                                              GError **error);
static gsize          wdf_read_block_header  (const guchar *buffer,
                                              gsize size,
                                              WdfBlock *header,
                                              GError **error);
static void           wdf_read_maparea_block (const guchar *buffer,
                                              WdfMapArea *maparea);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Renishaw WiRE data files (WDF)."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.3",
    "Daniil Bratashov (dn2010), David Necas (Yeti), Renishaw plc.",
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
    WdfFile filedata;
    WdfHeader fileheader;
    WdfBlock block;
    const guchar *p;
    gchar *title = NULL;
    GwyBrick *brick;
    GwyDataField *dfield;
    GwyDataLine *cal;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gdouble *ydata, *xdata, *data;
    gint i, j, k;
    gint xres, yres, zres;
    gint width, height, rowstride, bpp;
    gdouble xreal, yreal, zreal;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;
    guchar *pixels, *pix_p;

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

    filedata.whitelight = NULL;
    filedata.data = NULL;
    filedata.xlistdata = NULL;
    filedata.maparea = NULL;
    while (size > 0) {
        if ((len = wdf_read_block_header(p, size, &block, error)) == 0)
            goto fail;
        if (block.id == WDF_BLOCKID_DATA) {
            filedata.datasize = block.size - WDF_BLOCK_HEADER_SIZE;
            if (filedata.datasize != fileheader.npoints
                               * fileheader.nspectra * sizeof(gfloat)) {
                err_SIZE_MISMATCH(error,
                                  fileheader.npoints
                                * fileheader.nspectra * sizeof(gfloat),
                                  filedata.datasize, TRUE);
                goto fail;
            }
            filedata.data = (gfloat *)(p + WDF_BLOCK_HEADER_SIZE);
        }
        else if (block.id == WDF_BLOCKID_XLIST) {
            if (block.size != WDF_BLOCK_HEADER_SIZE
                            + 2 * sizeof(guint32)
                            + fileheader.npoints * sizeof(gfloat)) {
                err_SIZE_MISMATCH(error,
                                  WDF_BLOCK_HEADER_SIZE
                                  + 2 * sizeof(guint32)
                                  + fileheader.npoints * sizeof(gfloat),
                                  block.size, TRUE);
                goto fail;
            }
            p += WDF_BLOCK_HEADER_SIZE;
            filedata.xlisttype = gwy_get_guint32_le(&p);
            filedata.xlistunits = gwy_get_guint32_le(&p);
            filedata.xlistdata = (gfloat *)p;
            p -= WDF_BLOCK_HEADER_SIZE + 2 * sizeof(guint32);
        }
        else if (block.id == WDF_BLOCKID_ORIGIN) {
            gwy_debug("ORGN offset = %" G_GUINT64_FORMAT " size=%" G_GUINT64_FORMAT "",
                      ((guint64)p - (guint64)buffer), block.size);

        }
        else if (block.id == WDF_BLOCKID_MAPAREA) {
            if (block.size != WDF_MAP_AREA_SIZE) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("MapArea block is truncated"));
                goto fail;
            }
            filedata.maparea = g_new(WdfMapArea, 1);
            wdf_read_maparea_block(block.data, filedata.maparea);
        }
        else if (block.id == WDF_BLOCKID_WHITELIGHT) {
            loader = gdk_pixbuf_loader_new();
            if (!gdk_pixbuf_loader_write(loader, block.data, block.size,
                                                                &err)) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Pixbuf loader refused data: %s."),
                            err->message);
                g_clear_error(&err);
                g_object_unref(loader);
                goto fail;
            }
            gwy_debug("Closing the loader.");
            if (!gdk_pixbuf_loader_close(loader, &err)) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Pixbuf loader refused data: %s."),
                            err->message);
                g_clear_error(&err);
                g_object_unref(loader);
                goto fail;
            }
            gwy_debug("Trying to get the pixbuf.");
            pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
            gwy_debug("Pixbuf is: %p.", pixbuf);
            g_assert(pixbuf);
            g_object_ref(pixbuf);
            gwy_debug("Finalizing loader.");
            g_object_unref(loader);
            filedata.whitelight = pixbuf;
        }

        p += len;
        size -= len;
    }

    container = gwy_container_new();

    if (fileheader.nspectra == 1) { /* Single spectrum */
        zres = fileheader.npoints;
        if ((zres <= 0) || !(filedata.data) || !(filedata.xlistdata)) {
            err_FILE_TYPE(error, "Renishaw WDF");
            goto fail;
        }
        ydata = g_malloc(zres * sizeof(gdouble));
        gwy_convert_raw_data(filedata.data, zres, 1,
                             GWY_RAW_DATA_FLOAT,
                             GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             ydata, 1.0, 0.0);
        xdata = g_malloc(zres * sizeof(gdouble));
        gwy_convert_raw_data(filedata.xlistdata, zres, 1,
                             GWY_RAW_DATA_FLOAT,
                             GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             xdata, 1.0, 0.0);
        title = g_strdup(fileheader.title);
        gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL,
                             "title", title,
                       //    "si-unit-x", siunitx,
                       //    "si-unit-y", siunity,
                             NULL);
        gcmodel = g_object_new(GWY_TYPE_GRAPH_CURVE_MODEL,
                              "description", title,
                              "mode", GWY_GRAPH_CURVE_LINE,
                              "color", gwy_graph_get_preset_color(0),
                              NULL);
     // g_object_unref(siunitx);
     // g_object_unref(siunity);
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, zres);
        g_free(xdata);
        g_free(ydata);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_free(title);
        gwy_container_set_object_by_name(container, "/0/graph/graph/1",
                                         gmodel);
        g_object_unref(gmodel);
    }
    else { /* some kind of scan */
        if (!filedata.maparea) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("MapArea block is absent for scan"));
            goto fail;
        }

        if (filedata.maparea->length[2] != 1) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("3D Volume is unsupported now"));
            goto fail;
        }

        zres = fileheader.npoints;
        xres = filedata.maparea->length[0];
        yres = filedata.maparea->length[1];
        brick = gwy_brick_new(xres, yres, zres, xres, yres, zres, TRUE);
        data = gwy_brick_get_data(brick);
        p = (guchar *)filedata.data;

        for (i = 0; i < xres; i++)
            for (j = 0; j < yres; j++)
                for (k = 0; k < zres; k++) {
                    *(data + k * xres * yres + i + j * xres)
                                       = (gdouble)gwy_get_gfloat_le(&p);
                }

        cal = gwy_data_line_new(zres, zres, FALSE);
        data = gwy_data_line_get_data(cal);
        gwy_convert_raw_data(filedata.xlistdata, zres, 1,
                             GWY_RAW_DATA_FLOAT,
                             GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             data, 1.0, 0.0);
     // gwy_data_line_set_si_unit_y(cal, mdafile->siunitz);
        gwy_brick_set_zcalibration(brick, cal);
        g_object_unref(cal);

        gwy_container_set_object_by_name(container, "/brick/0", brick);
        title = g_strdup_printf("%s (WhiteLight)", fileheader.title);
        gwy_container_set_string_by_name(container, "/brick/0/title",
                                         title);
        dfield = gwy_data_field_new(xres, yres,
                                    xres, yres,
                                    TRUE);
        gwy_brick_mean_plane(brick, dfield, 0, 0, 0,
                             xres, yres, -1, FALSE);
        gwy_container_set_object_by_name(container, "/brick/0/preview",
                                         dfield);
        g_object_unref(dfield);
        g_object_unref(brick);

        gwy_file_volume_import_log_add(container, 0, NULL, filename);
    }

    if (filedata.whitelight) {
        pixbuf = filedata.whitelight;
        pixels = gdk_pixbuf_get_pixels(pixbuf);
        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);
        rowstride = gdk_pixbuf_get_rowstride(pixbuf);
        bpp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4 : 3;

        dfield = gwy_data_field_new(width, height, width, height, TRUE);
        data = gwy_data_field_get_data(dfield);
        for (i = 0; i < height; i++) {
            pix_p = pixels + i * rowstride;
            for (j = 0; j < width; j++) {
                guchar red = pix_p[bpp*j];
                guchar green = pix_p[bpp*j+1];
                guchar blue = pix_p[bpp*j+2];

                *(data + i * width + j) = (0.2126 * red
                                         + 0.7152 * green
                                         + 0.0722 * blue) / 255.0;
            }
        }
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        title = g_strdup_printf("%s (WhiteLight)", fileheader.title);
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         title);
        gwy_file_channel_import_log_add(container, 1, NULL, filename);
    }

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
    for (i = 0; i < 32; i++) {
        header->user[i] = *(buffer++);
    }
    for (i = 0; i < 160; i++) {
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

static void
wdf_read_maparea_block(const guchar *buffer,
                       WdfMapArea *maparea)
{
    gint i;

    maparea->flags  = gwy_get_guint32_le(&buffer);
    gwy_debug("flags=%d", maparea->flags);
    maparea->unused = gwy_get_guint32_le(&buffer);
    for (i = 0; i < 3; i++)
        maparea->location[i] = gwy_get_gfloat_le(&buffer);
    gwy_debug("location=%g, %g %g",
              maparea->location[0],
              maparea->location[1],
              maparea->location[2]);
    for (i = 0; i < 3; i++)
        maparea->stepsize[i] = gwy_get_gfloat_le(&buffer);
    gwy_debug("stepsize=%g, %g %g",
              maparea->stepsize[0],
              maparea->stepsize[1],
              maparea->stepsize[2]);
    for (i = 0; i < 3; i++)
        maparea->length[i] = gwy_get_guint32_le(&buffer);
    gwy_debug("length=%d, %d %d",
              maparea->length[0],
              maparea->length[1],
              maparea->length[2]);
    maparea->linefocus_size = gwy_get_guint32_le(&buffer);
    gwy_debug("linefocus_length=%d", maparea->linefocus_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
