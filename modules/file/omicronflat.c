/*
 *  $Id: spmlab.c 10739 2009-12-10 16:11:37Z yeti-dn $
 *  Copyright © 2009 François Bianco (fbianco) – Université de Genève, Suisse
 *  E-mail: francois.bianco@unige.ch
 *
 *  Partially based on code of omicronmatrix.c
 *  by Philipp Rahe and David Nečas (Yeti)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 or 3 of the License, or
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
 *
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-omicron-flat">
 *   <comment>Omicron flat file format</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="FLAT0100"/>
 *   </magic>
 *   <glob pattern="*.*_flat"/>
 *   <glob pattern="*.*_FLAT"/>
 * </mime-type>
 **/

#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC "FLAT"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define FLAT_VERSION "0100"
#define FLAT_VERSION_SIZE (sizeof(FLAT_VERSION)-1)

#define EXTENSION_HEADER "_flat"

#define STRING_MAXLENGTH 10000

#define TFF_LINEAR1D_NAME "TFF_Linear1D"
#define TFF_MULTILINEAR1D_NAME "TFF_MultiLinear1D"



// TODO : prehaps store these information in a gwy_container instead of using
// macro or represent the information differently
#define MIRRORED_NONE \
    (!gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored") \
     && !gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored"))

#define MIRRORED_BOTH \
    (gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored") \
     && gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored"))

#define MIRRORED_ONLY_X \
    (gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored") \
     && !gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored"))

#define MIRRORED_ONLY_Y \
    (!gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored") \
     && gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored"))

#define IS_2DIMAGE \
    (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
     && 3 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

#define IS_CITS \
    (2 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
     && ((4 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type") \
          || 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/1/type")) \
         && (4 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/1/type") \
             || 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))))

#define IS_SPS \
    (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
     && 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

#define IS_FORCE_DIST \
     (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
      && 6 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

#define IS_TIME_VARYING \
     (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count")\
      && 2 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

static gboolean      module_register   (void);
static gint          omicronflat_detect(const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* omicronflat_load  (const gchar *filename,
                                        G_GNUC_UNUSED GwyRunType mode,
                                        GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron flat files."),
    "fbianco  < francois.bianco@unige.ch > ",
    "0.1",
    "François Bianco",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("omicronflat",
                           N_("Omicron flat files (*.*_flat)"),
                           (GwyFileDetectFunc)&omicronflat_detect,
                           (GwyFileLoadFunc)&omicronflat_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
omicronflat_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase,
                                EXTENSION_HEADER) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && 0 == memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
         return 100;

    return 0;
}

/** read a string from the paramter or data file
 *  remember to free the result! */
static guchar*
flat_readstring(const guchar** fp, const gsize fp_end, GError **error)
{
    guchar* str = NULL;
    // len is the number of characters encoded, each is 16 bits length
    gsize len;
    GError* tmperr = NULL;

    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return NULL;
    }

    len = gwy_get_guint32_le(fp);

    if (len == 0) // empty string
        return NULL;

    if ((gsize)*fp + 2*len > fp_end || len > STRING_MAXLENGTH) {
        gwy_debug("omicronflat::flat_readstring: len > STRING_MAXLENGTH "
                  "or buffer too short, string not readable");
        err_FILE_TYPE(error, "Omicron Flat");
        return NULL;
    }

    str = (guchar*)g_utf16_to_utf8((gunichar2*)*fp, len, NULL, NULL, &tmperr);

    if (tmperr != NULL) {
        gwy_debug("omicronflat::flat_readstring: error reading or converting "
                  "string");
        g_propagate_error(error, tmperr);
    }

    // advance by length in gchar
    *fp += 2*len;
    return str;
}

static GwyContainer*
omicronflat_load(const gchar *filename, G_GNUC_UNUSED GwyRunType mode, GError **error)
{
    GError *tmperr = NULL;
    GwyContainer *metainfo = NULL; // store meta informations about the data structure
    GwyContainer *metadata = NULL; // store meta informations about the measurements
    GwyContainer *tffParams = NULL;
    GwyContainer *container = NULL;

    // Image data
    // GwyDataField for TraceUp, ReTraceUp, TraceDown, ReTraceDown
    // FIXME adapt and/or expand for CITS and SPS
    GwyDataField* dfield_tup = NULL;
    GwyDataField* dfield_retup = NULL;
    GwyDataField* dfield_tdown = NULL;
    GwyDataField* dfield_retdown = NULL;
    // as well as pointer
    gdouble* data_tup = NULL;
    gdouble* data_retup = NULL;
    gdouble* data_tdown = NULL;
    gdouble* data_retdown = NULL;
    // and indices
    guint32 ind_tup, ind_retup, ind_tdown, ind_retdown;
    guint32 xres, yres, maxint, cntl, cntp, n, avail;
    gdouble width, height;
    const guchar* sunit;
    gdouble fac = 1.;
    gdouble offset = 0.;

    guchar* file_buffer = NULL;
    const guchar* fp = NULL;
    gsize file_buffer_size;
    gsize fp_end;
    GError *err = NULL;
    gchar *error_msg = NULL;

    guchar* s = NULL;
    gchar key[100];
    gchar val[30];
    guint i,j,k,max,max2,max3;
    guint v;
    gdouble d;

    time_t timestamp;
    gchar creation_time[40];
    struct tm* sdate = NULL;

    guint param_count,type_code;
    guchar* instance_name = NULL;
    guchar* unit_str = NULL;
    guchar* name_str = NULL;
    guchar* value = NULL;

    gwy_debug("Reading %s", filename);
    if (!gwy_file_get_contents(filename, &file_buffer, &file_buffer_size,
                               &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    fp = file_buffer;
    fp_end = (gsize)file_buffer + file_buffer_size;

    if (file_buffer_size < MAGIC_SIZE + FLAT_VERSION_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);
        return NULL;
    }

    if (0 != memcmp(fp, MAGIC, MAGIC_SIZE)) {
        err_FILE_TYPE(error, "Omicron Flat");
        gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);
        return NULL;
    }
    fp += MAGIC_SIZE;

    if (0 != memcmp(fp, FLAT_VERSION, FLAT_VERSION_SIZE)) {
        err_FILE_TYPE(error, "Omicron Flat");
        gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);
        return NULL;
    }
    fp += FLAT_VERSION_SIZE;

    metainfo = gwy_container_new();
    metadata = gwy_container_new();

    if (fp_end < (gsize)fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }

    max = gwy_get_guint32_le(&fp);
    gwy_container_set_int32_by_name(metainfo, "axis_count", max);
    gwy_debug("AxisCount %i", max);

    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "/axis/%i/name", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         flat_readstring(&fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            goto fail;
        }
        gwy_debug("AxisName %s",
                  gwy_container_get_string_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/trigger", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         flat_readstring(&fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            goto fail;
        }
        gwy_debug("AxisTrigger %s",
                  gwy_container_get_string_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/units", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         flat_readstring(&fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            goto fail;
        }
        gwy_debug("AxisUnits %s",
                  gwy_container_get_string_by_name(metainfo, key));

        if (fp_end < (gsize)fp + 36) { // 5 × int32 + 2 × double (8 octets)
            err_FILE_TYPE(error, "Omicron Flat");
            goto fail;
        }

        g_snprintf(key, sizeof(key), "/axis/%i/clockCount", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(&fp));
        gwy_debug("AxisClockCount %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/startValue", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(&fp));
        gwy_debug("AxisStartValue %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/increment", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(&fp));
        gwy_debug("AxisIncrement %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/startValuePhysical", i);
        gwy_container_set_double_by_name(metainfo, key, gwy_get_gdouble_le(&fp));
        gwy_debug("AxisStartValuePhysical %11.4e",
                  gwy_container_get_double_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/incrementPhysical", i);
        gwy_container_set_double_by_name(metainfo, key, gwy_get_gdouble_le(&fp));
        gwy_debug("AxisIncrementPhysical %11.4e",
                  gwy_container_get_double_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/mirrored", i);
        gwy_container_set_boolean_by_name(metainfo, key,
                                          gwy_get_guint32_le(&fp) == 1);
        gwy_debug("Mirrored %i",
                  gwy_container_get_boolean_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/tablesCount", i);
        max2 = gwy_get_guint32_le(&fp);
        gwy_container_set_int32_by_name(metainfo, key, max2);
        gwy_debug("AxisTablesCount %i", max2);

        for (j = 0; j < max2; ++j) {
            gwy_debug("AxisTables %i", j);

            g_snprintf(key, sizeof(key), "/axis/%i/table/%i/trigger", i, j);
            gwy_container_set_string_by_name(metainfo, key,
                                             flat_readstring(&fp, fp_end, &tmperr));
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                goto fail;
            }
            gwy_debug("Trigger %s",
                      gwy_container_get_string_by_name(metainfo, key));

            if (fp_end < (gsize)fp + 4) {
                err_FILE_TYPE(error, "Omicron Flat");
                goto fail;
            }

            max3 = gwy_get_guint32_le(&fp);

            g_snprintf(key, sizeof(key), "/axis/%i/table/%i/interval_count",
                       i, j);
            gwy_container_set_int32_by_name(metainfo, key, max3);
            gwy_debug("Interval count %i", max3);

            // 3 × int32 × interval_count
            if (fp_end < (gsize)fp + (4*3*max3)) {
                err_FILE_TYPE(error, "Omicron Flat");
                goto fail;
            }

            for (k = 0; k < max3; ++k) {
                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/start", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(&fp));

                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/stop", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(&fp));

                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/step", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(&fp));
            }
        }
    }

    gwy_container_set_string_by_name(metainfo, "/channel/name",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_debug("ChannelName %s",
              gwy_container_get_string_by_name(metainfo, "/channel/name"));

    tffParams = gwy_container_new();
    s = flat_readstring(&fp, fp_end, &tmperr);
    if (tmperr != NULL) {
        g_free(s);
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_debug("TransferFunctionName %s", s);
    gwy_container_set_string_by_name(tffParams, "tff_name", s);
    g_free(s);

    gwy_container_set_string_by_name(metainfo, "/channel/units",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_debug("ChannelUnits %s",
              gwy_container_get_string_by_name(metainfo, "/channel/units"));

    if (fp_end < (gsize)fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }

    max = gwy_get_guint32_le(&fp);
    gwy_debug("ParamsCount %i", max);
    for (i = 0; i < max; ++i) {
        s = flat_readstring(&fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_free(s);
            g_propagate_error(error, tmperr);
            goto fail;
        }

        if (fp_end < (gsize)fp + 8) {
            g_free(s);
            err_FILE_TYPE(error, "Omicron Flat");
            goto fail;
        }

        d = gwy_get_gdouble_le(&fp);
        gwy_debug("%s = %11.4e", s, d);
        gwy_container_set_double_by_name(tffParams, (gchar*)s, d);
        g_free(s);
    }

    if (fp_end < (gsize)fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    max = gwy_get_guint32_le(&fp);
    gwy_container_set_int32_by_name(metainfo, "/channel/dataView/count", max);
    gwy_debug("DataViewCount %i", max);

    if (fp_end < (gsize)fp + 4*max) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "/channel/dataView/view/%i/type", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(&fp));
        gwy_debug("DataView %i",
                  gwy_container_get_int32_by_name(metainfo, key));
    }


    if (fp_end < (gsize)fp + 8) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    timestamp = (time_t) gwy_get_guint64_le(&fp);
    gwy_container_set_int64_by_name(metainfo, "timestamp", timestamp);
    sdate = localtime(&timestamp);
    strftime(creation_time, sizeof(creation_time), "%H:%M:%S %d.%m.%Y", sdate);
    gwy_container_set_string_by_name(metainfo, "creation_time",
                                     (guchar*)g_strdup(creation_time));
    gwy_debug("Creation time %s", creation_time);

    gwy_container_set_string_by_name(metainfo, "comment",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_debug("Comment %s",
              gwy_container_get_string_by_name(metainfo, "comment"));

    if (fp_end < (gsize)fp + 8) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    // expected data size
    gwy_container_set_int32_by_name(metainfo, "brickletSize",
                                    gwy_get_guint32_le(&fp));
    // measured data size
    gwy_container_set_int32_by_name(metainfo, "dataItemSize",
                                    gwy_get_guint32_le(&fp));

    gwy_debug("%i of %i data bricklets measured.",
              gwy_container_get_int32_by_name(metainfo, "dataItemSize"),
              gwy_container_get_int32_by_name(metainfo, "brickletSize"));

    // Try to figure out the file contents from the dataView value returned
    // See Omicron Vernissage MATRIX Result File Access and Export manual p.20
    //
    // If the file contains "topography images, current images and similar 2D data"
    if (IS_2DIMAGE) {
        xres = gwy_container_get_int32_by_name(metainfo, "/axis/0/clockCount")
               /(gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored")+1);
        yres = gwy_container_get_int32_by_name(metainfo, "/axis/1/clockCount")
               /(gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored")+1);
        width = gwy_container_get_double_by_name(metainfo, "/axis/0/incrementPhysical") * xres;
        height = gwy_container_get_double_by_name(metainfo, "/axis/1/incrementPhysical") * yres;
    }
    // Else if the file contains "planes form a volume CITS data cube"
    else if (IS_CITS) {
        // FIXME
        gwy_debug("File is planes form a volume CITS data cube ");
        error_msg = _("constant current tunnel spectroscopy (CITS)");
        err_DATA_TYPE(error, *error_msg );
        g_free(error_msg);
        goto fail;
    }
    // Else if the file contains "spectroscopy curves (SPS)"
    else if (IS_SPS) {
        // FIXME
        gwy_debug("File is spectroscopy curves (SPS) ");
        error_msg = _("single point spectroscopy (SPS)");
        err_DATA_TYPE(error, *error_msg );
        g_free(error_msg);
        goto fail;
    }
    // Else if the file contains "Force/distance curves" (1D data)
    else if (IS_FORCE_DIST) {
        // FIXME
        gwy_debug("File is force/distance curves ");
        error_msg = _("force/distance curves");
        err_DATA_TYPE(error, *error_msg );
        g_free(error_msg);
        goto fail;
    }
    // Else if the file contains "Temporally varying signal acquired over time"
    else if (IS_TIME_VARYING) {
        // FIXME
        gwy_debug("File is Temporally varying signal acquired over time ");
        error_msg = _("temporally varying signal acquired over time");
        err_DATA_TYPE(error, *error_msg );
        g_free(error_msg);
        goto fail;
    }
    else {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }

    dfield_tup = gwy_data_field_new(xres, yres,
                                    width, height, FALSE);
    data_tup = gwy_data_field_get_data(dfield_tup);
    dfield_retup = gwy_data_field_new(xres, yres,
                                      width, height, FALSE);
    data_retup = gwy_data_field_get_data(dfield_retup);
    dfield_tdown = gwy_data_field_new(xres, yres,
                                      width, height, FALSE);
    data_tdown = gwy_data_field_get_data(dfield_tdown);
    dfield_retdown = gwy_data_field_new(xres, yres,
                                        width, height, FALSE);
    data_retdown = gwy_data_field_get_data(dfield_retdown);
    ind_tup = xres * (yres-1);
    ind_retup = xres*yres - 1;
    ind_tdown = 0;
    ind_retdown = xres - 1;
    maxint = 4 * xres * yres;
    avail = gwy_container_get_int32_by_name(metainfo, "dataItemSize");
    n = 0;

    // Get correct scaling factor
    //   - TFF_LINEAR1D
    if (0 == strcmp((gchar*)gwy_container_get_string_by_name(
       tffParams, "tff_name"), TFF_LINEAR1D_NAME)) {
        gwy_debug("TransferFunctionType is linear1d");
        fac = 1. / gwy_container_get_double_by_name(tffParams, "Factor");
        offset = gwy_container_get_double_by_name(tffParams, "Offset");
    }
    //   - TFF_MULTILINEAR1D
    else if (0 == strcmp((gchar*)gwy_container_get_string_by_name(
            tffParams, "tff_name"), TFF_MULTILINEAR1D_NAME)) {
        gwy_debug("TransferFunctionType is multilinear1d");
        offset = gwy_container_get_double_by_name(tffParams, "Offset");

        // map multilinear1d paramters to linear1d factor
        fac = (gwy_container_get_double_by_name(tffParams, "Raw_1")
               - gwy_container_get_double_by_name(tffParams, "PreOffset"))
              /(gwy_container_get_double_by_name(tffParams, "NeutralFactor")
                * gwy_container_get_double_by_name(tffParams, "PreFactor"));
    }
    //   - UNKNOWN Transfer Function is used
    else {
        // setting factor and offset to 1.0 and 0.0 to obtain unscaled data
        g_warning("Unknown transfer function, "
                  "importing raw data without scaling.");
        fac = 1.;
        offset = 0.;
    }

    if (fp_end < (gsize)fp + avail*4) { // avail = number of point, each gint32
        err_TOO_SHORT(error);
        goto fail;
    }

    if (IS_2DIMAGE) {
        sunit = gwy_container_get_string_by_name(metainfo, "/axis/0/units");
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_xy(dfield_tup), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_xy(dfield_retup), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_xy(dfield_tdown), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_xy(dfield_retdown), (gchar*)sunit);

        sunit = gwy_container_get_string_by_name(metainfo, "/channel/units");
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_z(dfield_tup), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_z(dfield_retup), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_z(dfield_tdown), (gchar*)sunit);
        gwy_si_unit_set_from_string
            (gwy_data_field_get_si_unit_z(dfield_retdown), (gchar*)sunit);

        container = gwy_container_new();

        if (MIRRORED_NONE) {
            gwy_debug("None of the axis mirrored");

            for (cntl = 0; cntl < yres; ++cntl) {
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Trace Up
                    data_tup[ind_tup] = fac * (gwy_get_gint32_le(&fp)-offset);
                    ++ind_tup;
                    ++n;
                }
                ind_tup -= 2*xres;
            }
        }
        else if (MIRRORED_ONLY_X) {
            gwy_debug("Only x axis mirrored");

            for (cntl = 0; cntl < yres; ++cntl) {
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Trace Up
                    data_tup[ind_tup] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    ++ind_tup;
                    ++n;
                }
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Retrace Up
                    data_retup[ind_retup] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    ind_retup--;
                    ++n;
                }
                ind_tup -= 2*xres;
            }
        }
        else { //  == = MIRRORED_BOTH or MIRRORED_ONLY_Y
            gwy_debug("Either both or only y axis mirrored");
            for (cntl = 0; cntl < yres; ++cntl) {
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Trace Up
                    data_tup[ind_tup] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    ++ind_tup;
                    ++n;
                }
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Retrace Up
                    data_retup[ind_retup] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    --ind_retup;
                    ++n;
                }
                ind_tup -= 2*xres;
            }
            for (cntl = 0; cntl < yres; ++cntl) {
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Trace Down
                    data_tdown[ind_tdown] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    ++ind_tdown;
                    ++n;
                }
                for (cntp = 0; cntp < xres && n < avail; ++cntp) {
                    // Retrace Down
                    data_retdown[ind_retdown] =
                            fac * (gwy_get_gint32_le(&fp)-offset);
                    --ind_retdown;
                    ++n;
                }
                ind_retdown += 2*xres;
            }
        }

        gwy_debug("Topography data successfully read");

        gwy_container_set_object_by_name(container, "/0/data",
                                         dfield_tup);
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         (guchar*)g_strdup("Trace Up"));

        if ((MIRRORED_ONLY_X) || (MIRRORED_BOTH)) {
            gwy_container_set_object_by_name(container, "/1/data",
                                             dfield_retup);
            gwy_container_set_string_by_name(container, "/1/data/title",
                                             (guchar*)g_strdup("reTrace Up"));
        }
        if ((MIRRORED_ONLY_Y) || (MIRRORED_BOTH)) {
            gwy_container_set_object_by_name(container, "/2/data",
                                             dfield_tdown);
            gwy_container_set_string_by_name(container, "/2/data/title",
                                             (guchar*)g_strdup("Trace down"));
        }
        if (MIRRORED_BOTH) {
            gwy_container_set_object_by_name(container, "/3/data",
                                             dfield_retdown);
            gwy_container_set_string_by_name(container, "/3/data/title",
                                             (guchar*)g_strdup("reTrace down"));
        }

    }


    // Measurements metadata

    if (fp_end < (gsize)fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    // Offsets
    max = gwy_get_guint32_le(&fp);

    if (fp_end < (gsize)fp + 8) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "Offset:%i:x", i);
        v = gwy_get_guint32_le(&fp);
        g_snprintf(val, sizeof(val), "%i", v);
        gwy_container_set_string_by_name(metadata, key,(guchar*)g_strdup(val));

        g_snprintf(key, sizeof(key), "Offset:%i:y", i);
        v = gwy_get_guint32_le(&fp);
        g_snprintf(val, sizeof(val), "%i", v);
        gwy_container_set_string_by_name(metadata, key,(guchar*)g_strdup(val));
    }

    // Experiment informations
    gwy_container_set_string_by_name(metadata, "Info:name",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:version",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:description",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:fileSpecification",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:fileCreator",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:ResultFileCreator",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:UserName",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:AccountName",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }
    gwy_container_set_string_by_name(metadata, "Info:ResultFileSpecification",
                                     flat_readstring(&fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }

    if (fp_end < (gsize)fp + 12) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    v = gwy_get_guint32_le(&fp);
    g_snprintf(val, sizeof(val), "%i", v);
    gwy_container_set_string_by_name(metadata, "Info:runCycle",
                                     (guchar*)g_strdup(val));

    v = gwy_get_guint32_le(&fp);
    g_snprintf(val, sizeof(val), "%i", v);
    gwy_container_set_string_by_name(metadata, "Info:scanCycle",
                                     (guchar*)g_strdup(val));

    // Experiment element parameter list

    max = gwy_get_guint32_le(&fp);

    for (i = 0; i < max; ++i) {
        instance_name = flat_readstring(&fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            goto fail;
        }

        if (fp_end < (gsize)fp + 4) {
            err_FILE_TYPE(error, "Omicron Flat");
            goto fail;
        }
        param_count = gwy_get_guint32_le(&fp);
        gwy_debug("%i parameters in instance %s to read",
                  param_count, instance_name);

        for (j = 0; j < param_count; ++j) {
            name_str = flat_readstring(&fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                goto fail;
            }

            if (fp_end < (gsize)fp + 4) {
                err_FILE_TYPE(error, "Omicron Flat");
                goto fail;
            }
            type_code = gwy_get_guint32_le(&fp); // UNUSED
            // Type code info
            // --------------
            // 1 = 32 bits integer
            // 2 = double precision float
            // 3 = boolean ('True' or 'False')
            // 4 = Enum
            // 5 = Unicode characters string
            //
            // Values is encoded in a unicode string
            unit_str = flat_readstring(&fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_free(unit_str);
                g_propagate_error(error, tmperr);
                goto fail;
            }

            g_snprintf(key, sizeof(key), "Exp:%s:%s [%s]", 
                       instance_name, name_str, unit_str);
            g_free(unit_str);

            gwy_container_set_string_by_name(metadata, key,
                                             flat_readstring(&fp, fp_end, &tmperr));
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                goto fail;
            }
            gwy_debug("%s %s",
                      key, gwy_container_get_string_by_name(metadata, key));
            g_free(name_str);
            name_str = NULL;
        }
        g_free(instance_name);
        instance_name = NULL;
    }

    if (fp_end < (gsize)fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }
    // Deployement parameters
    max = gwy_get_guint32_le(&fp);

    for (i = 0; i < max; ++i) {
        instance_name = flat_readstring(&fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            goto fail;
        }

        if (fp_end < (gsize)fp + 4) {
            err_FILE_TYPE(error, "Omicron Flat");
            goto fail;
        }
        param_count = gwy_get_guint32_le(&fp);

        for (j = 0; j < param_count; ++j) {
            name_str = flat_readstring(&fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                goto fail;
            }
            value = flat_readstring(&fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                goto fail;
            }
            g_snprintf(key, sizeof(key), "Depl:%s:%s", instance_name, name_str);
            gwy_container_set_string_by_name(metadata, key, value);
            g_free(name_str);
            name_str = NULL;
        }
        g_free(instance_name);
        instance_name = NULL;
    }

#ifdef DEBUG
    if (fp_end == (gsize)fp)
        gwy_debug("File read, no data or information left.");
    else
        gwy_debug("Not all the data in the file was read.");
#endif

    gwy_container_set_object_by_name(container, "/0/meta", metadata);

    g_object_unref(dfield_tup);
    g_object_unref(dfield_retup);
    g_object_unref(dfield_tdown);
    g_object_unref(dfield_retdown);
    g_free(instance_name);
    g_free(unit_str);
    g_free(name_str);
    g_free(s);
    g_object_unref(metadata);
    g_object_unref(metainfo);
    gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);

    return container;

fail:
    gwy_debug("The file is either corrupted, or has an unknown/unhandled format. Module failed to read the file, you can blame the programmer… or help him…");

    gwy_object_unref(dfield_tup);
    gwy_object_unref(dfield_retup);
    gwy_object_unref(dfield_tdown);
    gwy_object_unref(dfield_retdown);
    g_free(instance_name);
    g_free(name_str);
    g_object_unref(metainfo);
    g_object_unref(metadata);
    g_free(s);
    gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
