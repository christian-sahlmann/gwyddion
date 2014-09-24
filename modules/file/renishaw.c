/*
 *  $Id$
 *  Copyright (C) 2014 Daniil Bratashov (dn2010).
 *  Data structures are copyright (c) 2011 Renishaw plc.
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
 
 /**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-renishaw-spm">
 *   <comment>Renishaw Wire Data File</comment>
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
 * 0 string WDF1 Renishaw Wire Data File
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Renishaw Wire Data File
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
	WDF_HEADER_SIZE = 512
};

typedef struct {
    guint32 id;
    guint32 uid;
    guint64 size;
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
    guint64 ncollected;  /* number of spectra written into the file (count) */
    guint32 naccum;      /* number of accumulations per spectrum */
    guint32 ylistcount;  /* number of elements in the y-list (>1 for image) */
    guint32 xlistcount;  /* number of elements for the x-list */
    guint32 origincount; /* number of data origin lists */
    gchar   appname[24];   /* application name (utf-8 encoded) */
    guint16 appversion[4]; /* application version (major,minor,patch,build) */
    guint32 scantype;    /* scan type - WdfScanType enum  */
    guint32 type;        /* measurement type - WdfType enum  */
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

static gboolean       module_register        (void);
static gint           wdf_detect      		 (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*  wdf_load               (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gsize		  wdf_read_header (const guchar *buffer,
									   gsize size,
									   WdfHeader *header,
									   GError **error);                                              

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Renishaw Wire data files (WDF)."),
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
                           N_("Renishaw Wire data files (.wdf)"),
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
    gdouble *data;
    WdfHeader fileheader;
    gchar *p; 
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
	fprintf(stderr,"%d %d\n", fileheader.npoints, fileheader.nspectra);
	fprintf(stderr,"x=%d y=%d\n", fileheader.xlistcount, fileheader.ylistcount);

	zres = fileheader.npoints;
	brick = gwy_brick_new(xres, yres, zres, xreal, yreal, zreal, TRUE);
	data = gwy_brick_get_data(brick);
	
	p = buffer + 528;

    for (i = 0; i < xres; i++)
		for (j = 0; j < yres; j++) 
			for (k = 0; k < zres; k++) {
                *(data + k * xres * yres + i + j * xres)
									   = (gdouble)gwy_get_gfloat_le(&p);
            }
    
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
    header->size	    = gwy_get_guint64_le(&buffer);
    header->flags	    = gwy_get_guint64_le(&buffer);
    for (i = 0; i < 4; i++) {
		header->uuid[i] = gwy_get_guint32_le(&buffer);
	}
    header->unused0     = gwy_get_guint64_le(&buffer);
    header->unused1     = gwy_get_guint32_le(&buffer);
    header->ntracks		= gwy_get_guint32_le(&buffer);
    header->status		= gwy_get_guint32_le(&buffer);
    header->npoints		= gwy_get_guint32_le(&buffer);
    header->nspectra	= gwy_get_guint64_le(&buffer);
    header->ncollected	= gwy_get_guint64_le(&buffer);
    header->naccum      = gwy_get_guint32_le(&buffer);
    header->ylistcount  = gwy_get_guint32_le(&buffer);
    header->xlistcount  = gwy_get_guint32_le(&buffer);
    header->origincount	= gwy_get_guint32_le(&buffer);
    for (i = 0; i < 24; i++) {
		header->appname[i] = *(buffer++);
	}
    for (i = 0; i < 4; i++) {
		header->appversion[i] = gwy_get_guint16_le(&buffer);
	}
    header->scantype	= gwy_get_guint32_le(&buffer);
    header->type		= gwy_get_guint32_le(&buffer);
    header->time_start	= gwy_get_guint64_le(&buffer);
    header->time_end	= gwy_get_guint64_le(&buffer);
    header->units		= gwy_get_guint32_le(&buffer);
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
