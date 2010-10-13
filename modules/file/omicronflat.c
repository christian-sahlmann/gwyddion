/*
 *  $Id$
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * Omicron flat format
 * .*_flat
 * Read
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

// Image type macros
#define IS_2DIMAGE \
    (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
     && 3 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

// Even Omicron is ambiguous how they define cits map with dataview type …
#define IS_CITS \
    ( ( 2 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
      && \
       ( \
        (4 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type") \
        && 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/1/type")) \
       || \
        (4 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/1/type") \
        && 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type")) \
       ) \
      ) \
    || \
      ( 1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
      && (4 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type")) \
      )\
    )

#define IS_SPS \
    (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
     && 5 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

#define IS_FORCE_DIST \
     (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count") \
      && 6 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))

#define IS_TIME_VARYING \
     (1 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/count")\
      && 2 == gwy_container_get_int32_by_name(metainfo, "/channel/dataView/view/0/type"))


static gboolean      module_register    (void);
static gint          omicronflat_detect (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static guchar*       omicronflat_readstring(const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_readmetainfo(GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_readmetadata(GwyContainer *metadata,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_getscalingfactor(GwyContainer *metainfo,
                                        gdouble* fac, gdouble* offset);
static void          omicronflat_read2dimage(GwyContainer *container,
                                        GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_readcits(GwyContainer *container,
                                        GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_readsps(GwyContainer *container,
                                        GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
#ifdef omicronflat_todo
static void          omicronflat_readforcedist(GwyContainer *container,
                                        GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
static void          omicronflat_readtimevarying(GwyContainer *container,
                                        GwyContainer *metainfo,
                                        const guchar **fp, const gsize fp_end,
                                        GError **error);
#endif
static GwyContainer* omicronflat_load   (const gchar *filename,
                                        G_GNUC_UNUSED GwyRunType mode,
                                        GError **error);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron flat files."),
    "fbianco  < francois.bianco@unige.ch > ",
    "0.2",
    "François Bianco",
    "2010",
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

/**
 * omicronflat_readstring:
 *
 * Remember to free the result!
 *
 * Returns : The current string from the file buffer position
 *
 **/
static guchar*
omicronflat_readstring(const guchar** fp, const gsize fp_end, GError **error)
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
        gwy_debug("omicronflat::omicronflat_readstring: len > STRING_MAXLENGTH "
                  "or buffer too short, string not readable");
        err_FILE_TYPE(error, "Omicron Flat");
        return NULL;
    }

    str = (guchar*)g_utf16_to_utf8((gunichar2*)*fp, len, NULL, NULL, &tmperr);

    if (tmperr != NULL) {
        gwy_debug("omicronflat::omicronflat_readstring: error reading or converting "
                  "string");
        g_propagate_error(error, tmperr);
    }

    // advance by length in gchar
    *fp += 2*len;
    return str;
}

/**
 * omicronflat_readmetainfo:
 *
 * Reads the metainfo, i.e information about the data structure
 *
 **/
static void
omicronflat_readmetainfo(GwyContainer *metainfo, const guchar** fp,
                         const gsize fp_end, GError **error)
{
    GError* tmperr = NULL;

    gchar key[100];
    guchar* s = NULL;

    time_t timestamp;
    gchar creation_time[40];
    struct tm* sdate = NULL;

    guint32 i,j,k,max,max2,max3;
    gdouble val;

    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    max = gwy_get_guint32_le(fp);
    gwy_container_set_int32_by_name(metainfo, "axis_count", max);
    gwy_debug("AxisCount %i", max);

    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "/axis/%i/name", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         omicronflat_readstring(fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            return;
        }
        gwy_debug("AxisName %s",
                  gwy_container_get_string_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/trigger", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         omicronflat_readstring(fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            return;
        }
        gwy_debug("AxisTrigger %s",
                  gwy_container_get_string_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/units", i);
        gwy_container_set_string_by_name(metainfo, key,
                                         omicronflat_readstring(fp, fp_end, &tmperr));
        if (tmperr != NULL) {
            g_propagate_error(error, tmperr);
            return;
        }
        gwy_debug("AxisUnits %s",
                  gwy_container_get_string_by_name(metainfo, key));

        if (fp_end < (gsize)*fp + 36) { // 5 × int32 + 2 × double (8 octets)
            err_FILE_TYPE(error, "Omicron Flat");
            return;
        }

        g_snprintf(key, sizeof(key), "/axis/%i/clockCount", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(fp));
        gwy_debug("AxisClockCount %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/startValue", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(fp));
        gwy_debug("AxisStartValue %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/increment", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(fp));
        gwy_debug("AxisIncrement %i",
                  gwy_container_get_int32_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/startValuePhysical", i);
        gwy_container_set_double_by_name(metainfo, key, gwy_get_gdouble_le(fp));
        gwy_debug("AxisStartValuePhysical %11.4e",
                  gwy_container_get_double_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/incrementPhysical", i);
        gwy_container_set_double_by_name(metainfo, key, gwy_get_gdouble_le(fp));
        gwy_debug("AxisIncrementPhysical %11.4e",
                  gwy_container_get_double_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/mirrored", i);
        gwy_container_set_boolean_by_name(metainfo, key,
                                          gwy_get_guint32_le(fp) == 1);
        gwy_debug("Mirrored %i",
                  gwy_container_get_boolean_by_name(metainfo, key));

        g_snprintf(key, sizeof(key), "/axis/%i/tablesCount", i);

        max2 = gwy_get_guint32_le(fp);
        gwy_container_set_int32_by_name(metainfo, key, max2);
        gwy_debug("AxisTablesCount %i", max2);

        for (j = 0; j < max2; ++j) {
            gwy_debug("AxisTables %i", j);

            g_snprintf(key, sizeof(key), "/axis/%i/table/%i/trigger", i, j);
            gwy_container_set_string_by_name(metainfo, key,
                                             omicronflat_readstring(fp, fp_end, &tmperr));
            if (tmperr != NULL) {
                g_propagate_error(error, tmperr);
                return;
            }
            gwy_debug("Trigger %s",
                      gwy_container_get_string_by_name(metainfo, key));

            if (fp_end < (gsize)*fp + 4) {
                err_FILE_TYPE(error, "Omicron Flat");
                return;
            }
            max3 = gwy_get_guint32_le(fp);

            g_snprintf(key, sizeof(key), "/axis/%i/table/%i/intervalCount",
                       i, j);
            gwy_container_set_int32_by_name(metainfo, key, max3);
            gwy_debug("Interval count %i", max3);

            // 3 × int32 × interval_count
            if (fp_end < (gsize)*fp + (12*max3)) {
                err_FILE_TYPE(error, "Omicron Flat");
                return;
            }

            for (k = 0; k < max3; ++k) {
                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/start", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(fp));

                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/stop", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(fp));

                g_snprintf(key, sizeof(key),
                           "/axis/%i/table/%i/interval/%i/step", i, j, k);
                gwy_container_set_int32_by_name(metainfo, key,
                                                gwy_get_guint32_le(fp));
            }
        }
    }

    gwy_container_set_string_by_name(metainfo, "/channel/name",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_debug("ChannelName %s",
              gwy_container_get_string_by_name(metainfo, "/channel/name"));

    s = omicronflat_readstring(fp, fp_end, &tmperr);
    if (tmperr != NULL) {
        g_free(s);
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_debug("TransferFunctionName %s", s); // How to scale the raw data
    gwy_container_set_string_by_name(metainfo, "/tff/name", s);
    s = NULL;

    gwy_container_set_string_by_name(metainfo, "/channel/units",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_debug("ChannelUnits %s",
              gwy_container_get_string_by_name(metainfo, "/channel/units"));

    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    max = gwy_get_guint32_le(fp);
    gwy_debug("ParamsCount %i", max);
    for (i = 0; i < max; ++i) {
        s = omicronflat_readstring(fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_free(s);
            g_propagate_error(error, tmperr);
            return;
        }

        if (fp_end < (gsize)*fp + 8) {
            g_free(s);
            err_FILE_TYPE(error, "Omicron Flat");
            return;
        }

        val = gwy_get_gdouble_le(fp);
        gwy_debug("%s = %5.4e", s, val);
        g_snprintf(key, sizeof(key), "/tff/%s", s);
        gwy_container_set_double_by_name(metainfo, key, val);
        g_free(s);
    }

    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }
    max = gwy_get_guint32_le(fp);
    gwy_container_set_int32_by_name(metainfo, "/channel/dataView/count", max);
    gwy_debug("DataViewCount %i", max);

    if (fp_end < (gsize)*fp + 4*max) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }
    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "/channel/dataView/view/%i/type", i);
        gwy_container_set_int32_by_name(metainfo, key, gwy_get_guint32_le(fp));
        gwy_debug("DataView %i",
                  gwy_container_get_int32_by_name(metainfo, key));
    }

    if (fp_end < (gsize)*fp + 8) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }
    timestamp = (time_t) gwy_get_guint64_le(fp);
    gwy_container_set_int64_by_name(metainfo, "timestamp", timestamp);
    sdate = localtime(&timestamp);
    strftime(creation_time, sizeof(creation_time), "%H:%M:%S %d.%m.%Y", sdate);
    gwy_container_set_string_by_name(metainfo, "creation_time",
                                     (guchar*)g_strdup(creation_time));
    gwy_debug("Creation time %s", creation_time);

    gwy_container_set_string_by_name(metainfo, "comment",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_debug("Comment %s",
              gwy_container_get_string_by_name(metainfo, "comment"));

    if (fp_end < (gsize)*fp + 8) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    gwy_container_set_int32_by_name(metainfo, "brickletSize", gwy_get_guint32_le(fp));
    gwy_container_set_int32_by_name(metainfo, "dataItemSize", gwy_get_guint32_le(fp));

    return;
}

/**
 * omicronflat_readmetadata:
 *
 * Reads the metadata, i.e information about the experiment
 *
 **/
static void
omicronflat_readmetadata(GwyContainer *metadata, const guchar** fp,
                         const gsize fp_end, GError **error)
{
    GError* tmperr = NULL;

    guint32 i,max,j,max2;
    gchar key[100];
    gchar val[30];

    guint32 type_code;
    guchar* instance_name = NULL;
    guchar* unit_str = NULL;
    guchar* name_str = NULL;
    guchar* value = NULL;

    // Offsets
    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    max = gwy_get_guint32_le(fp);
    gwy_debug("Number of offsets info %i", max);

    if (fp_end < (gsize)*fp + 16*max) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }
    for (i = 0; i < max; ++i) {
        g_snprintf(key, sizeof(key), "Offset:%i:x", i);
        gwy_container_set_double_by_name(metadata, key, gwy_get_gdouble_le(fp));

        g_snprintf(key, sizeof(key), "Offset:%i:y", i);
        gwy_container_set_double_by_name(metadata, key,gwy_get_gdouble_le(fp));
    }

    // Experiment informations
    gwy_container_set_string_by_name(metadata, "Info:name",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:version",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:description",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:fileSpecification",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:fileCreator",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:ResultFileCreator",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:UserName",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:AccountName",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }
    gwy_container_set_string_by_name(metadata, "Info:ResultFileSpecification",
                                     omicronflat_readstring(fp, fp_end, &tmperr));
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        return;
    }

    if (fp_end < (gsize)*fp + 12) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    g_snprintf(val, sizeof(val), "%i", gwy_get_guint32_le(fp));
    gwy_container_set_string_by_name(metadata, "Info:runCycle",
                                     (guchar*)g_strdup(val));

    g_snprintf(val, sizeof(val), "%i", gwy_get_guint32_le(fp));
    gwy_container_set_string_by_name(metadata, "Info:scanCycle",
                                     (guchar*)g_strdup(val));

    // Experiment element parameter list
    max = gwy_get_guint32_le(fp);

    for (i = 0; i < max; ++i) {
        instance_name = omicronflat_readstring(fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_free(instance_name);
            g_propagate_error(error, tmperr);
            return;
        }

        if (fp_end < (gsize)*fp + 4) {
            g_free(instance_name);
            err_FILE_TYPE(error, "Omicron Flat");
            return;
        }
        max2 = gwy_get_guint32_le(fp);
        gwy_debug("%i parameters in instance %s to read",
                  max2, instance_name);

        for (j = 0; j < max2; ++j) {
            name_str = omicronflat_readstring(fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_free(instance_name);
                g_free(name_str);
                g_propagate_error(error, tmperr);
                return;
            }

            if (fp_end < (gsize)*fp + 4) {
                g_free(instance_name);
                g_free(name_str);
                err_FILE_TYPE(error, "Omicron Flat");
                return;
            }
            type_code = gwy_get_guint32_le(fp); // UNUSED
            // Type code info
            // --------------
            // 1 = 32 bits integer
            // 2 = double precision float
            // 3 = boolean ('True' or 'False')
            // 4 = Enum
            // 5 = Unicode characters string
            //
            // Values is encoded in a unicode string
            unit_str = omicronflat_readstring(fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_free(instance_name);
                g_free(name_str);
                g_free(unit_str);
                g_propagate_error(error, tmperr);
                return;
            }

            g_snprintf(key, sizeof(key), "Exp:%s:%s [%s]",
                       instance_name, name_str, unit_str);

            gwy_container_set_string_by_name(metadata, key,
                                             omicronflat_readstring(fp, fp_end, &tmperr));
            if (tmperr != NULL) {
                g_free(instance_name);
                g_free(name_str);
                g_free(unit_str);
                g_propagate_error(error, tmperr);
                return;
            }
            gwy_debug("%s %s",
                      key, gwy_container_get_string_by_name(metadata, key));
            g_free(unit_str);
            g_free(name_str);
        }
        g_free(instance_name);
    }

    if (fp_end < (gsize)*fp + 4) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }
    // Deployement parameters
    max = gwy_get_guint32_le(fp);

    for (i = 0; i < max; ++i) {
        instance_name = omicronflat_readstring(fp, fp_end, &tmperr);
        if (tmperr != NULL) {
            g_free(instance_name);
            g_propagate_error(error, tmperr);
            return;
        }

        if (fp_end < (gsize)*fp + 4) {
            g_free(instance_name);
            err_FILE_TYPE(error, "Omicron Flat");
            return;
        }
        max2 = gwy_get_guint32_le(fp);

        for (j = 0; j < max2; ++j) {
            name_str = omicronflat_readstring(fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_free(instance_name);
                g_free(name_str);
                g_propagate_error(error, tmperr);
                return;
            }
            value = omicronflat_readstring(fp, fp_end, &tmperr);
            if (tmperr != NULL) {
                g_free(instance_name);
                g_free(name_str);
                g_free(value);
                g_propagate_error(error, tmperr);
                return;
            }
            g_snprintf(key, sizeof(key), "Depl:%s:%s", instance_name, name_str);
            gwy_container_set_string_by_name(metadata, key, value);

            value = NULL;
            g_free(name_str);
        }
        g_free(instance_name);
    }

    return;
}

/**
 * omicronflat_getscalingfactor:
 *
 * Gets the correct scaling factor and offset from the metainfo container
 * in the double fac(tor) and offset
 *
 **/
static void
omicronflat_getscalingfactor(GwyContainer *metainfo,
                             gdouble *fac, gdouble *offset)
{
    // Get correct scaling factor
    //   - TFF_LINEAR1D
    if (0 == strcmp((gchar*)gwy_container_get_string_by_name(
       metainfo, "/tff/name"), TFF_LINEAR1D_NAME)) {
        gwy_debug("TransferFunctionType is linear1d");
        *fac = 1. / gwy_container_get_double_by_name(metainfo, "/tff/Factor");
        *offset = gwy_container_get_double_by_name(metainfo, "/tff/Offset");
    }
    //   - TFF_MULTILINEAR1D
    else if (0 == strcmp((gchar*)gwy_container_get_string_by_name(
            metainfo, "/tff/name"), TFF_MULTILINEAR1D_NAME)) {
        gwy_debug("TransferFunctionType is multilinear1d");
        *offset = gwy_container_get_double_by_name(metainfo, "/tff/Offset");

        // map multilinear1d paramters to linear1d factor
        *fac = (gwy_container_get_double_by_name(metainfo, "/tff/Raw_1")
               - gwy_container_get_double_by_name(metainfo, "/tff/PreOffset"))
              /(gwy_container_get_double_by_name(metainfo, "/tff/NeutralFactor")
                * gwy_container_get_double_by_name(metainfo, "/tff/PreFactor"));
    }
    //   - UNKNOWN Transfer Function is used
    else {
        // setting factor and offset to 1.0 and 0.0 to obtain unscaled data
        g_warning("Unknown transfer function, "
                  "importing raw data without scaling.");
        *fac = 1.;
        *offset = 0.;
    }

    return;
}

/**
 * omicronflat_read2dimage:
 *
 * Reads the data from the file buffer and store it with the correct shape
 * as 2d image(s) into the data container.
 *
 **/
static void
omicronflat_read2dimage(GwyContainer *container, GwyContainer *metainfo,
                        const guchar **fp, const gsize fp_end, GError **error)
{
    guint32 i, j, n, navail, nexpect;
    guint32 ind_tup, ind_retup, ind_tdown, ind_retdown;
    guint32 xres, yres;
    gdouble width, height;
    const guchar* unit_xy;
    const guchar* unit_z;
    gboolean xmirrored, ymirrored;
    gdouble fac = 1.;
    gdouble offset = 0;

    GPtrArray* dfield_arr = NULL;
    GPtrArray* data_arr = NULL;
    GwyDataField* dfield = NULL;
    gdouble *data = NULL;

    nexpect = gwy_container_get_int32_by_name(metainfo,"brickletSize");
    navail = gwy_container_get_int32_by_name(metainfo,"dataItemSize");

    if (navail > nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    // Check buffer length for data available
    if (fp_end < (gsize)*fp + navail*4) { // navail = number of point, each gint32
        err_TOO_SHORT(error);
        return;
    }

    xmirrored = gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored");
    ymirrored = gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored");

    // How many images are stored in the file ?
    if (!xmirrored && !ymirrored) // not mirrored
        n = 1;
    else if (xmirrored && ymirrored) // both
        n = 4;
    else // only one axis mirrored
        n = 2;

    dfield_arr = g_ptr_array_sized_new(n);
    data_arr = g_ptr_array_sized_new(n);

    // number of points measured
    xres = gwy_container_get_int32_by_name(metainfo, "/axis/0/clockCount")
            /(xmirrored+1);
    yres = gwy_container_get_int32_by_name(metainfo, "/axis/1/clockCount")
            /(ymirrored+1);
    // physical size
    width = gwy_container_get_double_by_name(metainfo, "/axis/0/incrementPhysical") * xres;
    height = gwy_container_get_double_by_name(metainfo, "/axis/1/incrementPhysical") * yres;

    unit_xy = gwy_container_get_string_by_name(metainfo, "/axis/0/units");
    unit_z = gwy_container_get_string_by_name(metainfo, "/channel/units");

    // check data shape, n = mirroring factor
    if (xres*yres*n != nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    // create data field for all the scanning directions
    for (i = 0; i < n; ++i) {
        dfield = gwy_data_field_new(xres, yres,
                                    width, height, TRUE);
        data = gwy_data_field_get_data(dfield);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield),
                                            (gchar*)unit_xy);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield),
                                            (gchar*)unit_z);
        g_ptr_array_add(dfield_arr,dfield);
        g_ptr_array_add(data_arr,data);
    }

    ind_tup = xres * (yres-1);
    ind_retup = xres*yres - 1;
    ind_tdown = 0;
    ind_retdown = xres - 1;
    n = 0;

    omicronflat_getscalingfactor(metainfo, &fac, &offset);

    if (!xmirrored && !ymirrored) {
        gwy_debug("None of the axis mirrored");

        // Trace Up
        data = g_ptr_array_index(data_arr, 0);
        for (i = 0; i < yres; ++i) {
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_tup] =  fac * (gwy_get_gint32_le(fp)-offset);
                ++ind_tup;
                ++n;
            }
            ind_tup -= 2*xres;
        }
    }
    else if (xmirrored && !ymirrored) {
        gwy_debug("Only x axis mirrored");

        for (i = 0; i < yres; ++i) {
            // Trace Up
            data = g_ptr_array_index(data_arr, 0);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_tup] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                ++ind_tup;
                ++n;
            }
            // Retrace Up
            data = g_ptr_array_index(data_arr, 1);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_retup] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                --ind_retup;
                ++n;
            }
            ind_tup -= 2*xres;
        }
    }
    else {
        gwy_debug("Either both or only y axis mirrored");
        for (i = 0; i < yres; ++i) {
            // Trace Up
            data = g_ptr_array_index(data_arr, 0);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_tup] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                ++ind_tup;
                ++n;
            }
            // Retrace Up
            data = g_ptr_array_index(data_arr, 1);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_retup] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                --ind_retup;
                ++n;
            }
            ind_tup -= 2*xres;
        }
        for (i = 0; i < yres; ++i) {
            // Trace Down
            data = g_ptr_array_index(data_arr, 2);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_tdown] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                ++ind_tdown;
                ++n;
            }
            // Retrace Down
            data = g_ptr_array_index(data_arr, 3);
            for (j = 0; j < xres && n < navail; ++j) {
                data[ind_retdown] =
                        fac * (gwy_get_gint32_le(fp)-offset);
                --ind_retdown;
                ++n;
            }
            ind_retdown += 2*xres;
        }
    }

    // check if all data were read correctly
    if (n != navail) {
        err_FILE_TYPE(error, "Omicron Flat");
        g_ptr_array_free(data_arr,TRUE);
        g_ptr_array_free(dfield_arr,TRUE);
        return;
    }

    gwy_debug("Topography data successfully read.");

    dfield = g_ptr_array_index(dfield_arr, 0);
    gwy_container_set_object_by_name(container, "/0/data",
                                        dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                        (guchar*)g_strdup("Trace Up"));

    if (xmirrored) {
        dfield = g_ptr_array_index(dfield_arr, 1);
        gwy_container_set_object_by_name(container, "/1/data",
                                            dfield);
        gwy_container_set_string_by_name(container, "/1/data/title",
                                            (guchar*)g_strdup("reTrace Up"));
    }
    if (ymirrored) {
        dfield = g_ptr_array_index(dfield_arr, 2);
        gwy_container_set_object_by_name(container, "/2/data",
                                            dfield);
        gwy_container_set_string_by_name(container, "/2/data/title",
                                            (guchar*)g_strdup("Trace down"));
    }
    if (xmirrored && ymirrored) {
        dfield = g_ptr_array_index(dfield_arr, 3);
        gwy_container_set_object_by_name(container, "/3/data",
                                            dfield);
        gwy_container_set_string_by_name(container, "/3/data/title",
                                            (guchar*)g_strdup("reTrace down"));
    }

    g_ptr_array_free(data_arr,TRUE);
    g_ptr_array_free(dfield_arr,TRUE);

    return;
}

/**
 * omicronflat_readcits:
 *
 * Reads the data from the file buffer and store it with the correct shape
 * as CITS (3D maps) into the data container.
 *
 **/
static void
omicronflat_readcits(GwyContainer *container, GwyContainer *metainfo,
                     const guchar **fp, const gsize fp_end, GError **error)
{
    guint32 i, n, m, navail, nexpect;
    guint32 ind_tup, ind_retup, ind_tdown, ind_retdown;
    guint32 xres, yres, zres;
    gdouble width, height;
    const guchar* unit_xy;
    const guchar* unit_z;
    gboolean xmirrored, ymirrored, zmirrored, xspecmir, yspecmir;
    guint32 zmax, zmfac;
    gdouble zinc, zstart;
    guint32 ystart, ystop, ystep, xstart, xstop, xstep, cntx, cnty, cntz;
    gdouble fac = 1.;
    gdouble offset = 0;
    gchar key[30];
    gchar val[30];

    GPtrArray* dfield_arr = NULL;
    GPtrArray* data_arr = NULL;
    GwyDataField* dfield = NULL;
    gdouble *data = NULL;

    gwy_debug("File is planes form a volume CITS data cube ");

    nexpect = gwy_container_get_int32_by_name(metainfo,"brickletSize");
    navail = gwy_container_get_int32_by_name(metainfo,"dataItemSize");

    if (navail > nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    // Check buffer length for data available
    if (fp_end < (gsize)*fp + navail*4) { // navail = number of point, each gint32
        err_TOO_SHORT(error);
        return;
    }

    zmirrored = gwy_container_get_boolean_by_name(metainfo, "/axis/0/mirrored");
    xmirrored = gwy_container_get_boolean_by_name(metainfo, "/axis/1/mirrored");
    ymirrored = gwy_container_get_boolean_by_name(metainfo, "/axis/2/mirrored");
    xspecmir = FALSE;
    yspecmir = FALSE;

    xstart = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/0/interval/0/start");
    xstop = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/0/interval/0/stop");
    xstep = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/0/interval/0/step");
    xres = (xstop - xstart) / xstep + 1;

    if (gwy_container_get_int32_by_name(metainfo,"/axis/0/table/0/intervalCount") == 2)
        xspecmir = TRUE;

    ystart = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/1/interval/0/start");
    ystop = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/1/interval/0/stop");
    ystep = gwy_container_get_int32_by_name(metainfo, "/axis/0/table/1/interval/0/step");
    yres = (ystop - ystart)/ystep + 1;

    if (gwy_container_get_int32_by_name(metainfo,"/axis/0/table/1/intervalCount") == 2)
        yspecmir = TRUE;

    zres = gwy_container_get_int32_by_name(metainfo, "/axis/0/clockCount") / (zmirrored+1);
    zstart = gwy_container_get_double_by_name(metainfo, "/axis/0/startValuePhysical");
    zinc = gwy_container_get_double_by_name(metainfo, "/axis/0/incrementPhysical");

    if (xspecmir && yspecmir) // both spec axis mirrored
        zmfac = 4;
    else if (xspecmir || yspecmir)
        zmfac = 2;
    else // none
        zmfac = 1;

    // physical size
    width = gwy_container_get_double_by_name(metainfo, "/axis/1/incrementPhysical")
            * gwy_container_get_int32_by_name(metainfo, "/axis/1/clockCount")
            / (xmirrored+1);
    height = gwy_container_get_double_by_name(metainfo, "/axis/2/incrementPhysical")
            * gwy_container_get_int32_by_name(metainfo, "/axis/2/clockCount")
            / (ymirrored+1);

    unit_xy = gwy_container_get_string_by_name(metainfo, "/axis/1/units");
    unit_z = gwy_container_get_string_by_name(metainfo, "/channel/units");

    zmax = zres * (zmirrored+1);

    // check if data shape was understood correctly
    if (xres*yres*zmfac*zmax != nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    dfield_arr = g_ptr_array_sized_new(zmax * zmfac);
    data_arr = g_ptr_array_sized_new(zmax * zmfac);

    for (i = 0; i < zmax*zmfac; ++i) {
        dfield = gwy_data_field_new(xres, yres,
                                    width, height, TRUE);
        data = gwy_data_field_get_data(dfield);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield),
                                            (gchar*)unit_xy);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield),
                                            (gchar*)unit_z);
        g_ptr_array_add(dfield_arr,dfield);
        g_ptr_array_add(data_arr,data);
    }

    ind_tup = xres * (yres-1);
    ind_retup = xres * yres - 1;
    ind_tdown = 0;
    ind_retdown = xres - 1;
    n = 0;

    omicronflat_getscalingfactor(metainfo, &fac, &offset);

    if (!xspecmir && !yspecmir) { // no axis are mirrored
        gwy_debug("None of the axis mirrored");

        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz) {
                    data = g_ptr_array_index(data_arr,cntz);
                    data[ind_tup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tup;
            }
            ind_tup -= 2*xres;
        }
    }
    else if (xspecmir && !yspecmir) {
        gwy_debug("Only x axis mirrored");

        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz);
                    data[ind_tup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tup;
            }
            for (cntx = 0; cntx < xres; ++cntx) {
                // Retrace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz+zmax);
                    data[ind_retup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                --ind_retup;
            }
            ind_tup -= 2*xres;
        }
    }
    else if (yspecmir && !xspecmir){
        gwy_debug("Only y axis mirrored");

        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz) {
                    data = g_ptr_array_index(data_arr,cntz);
                    data[ind_tup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tup;
            }
            ind_tup -= 2*xres;
        }
        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Down
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz+zmax);
                    data[ind_tdown] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tdown;
            }
            ind_retdown += 2*xres;
        }
    }
    else {
        gwy_debug("Both axis mirrored");
        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz) {
                    data = g_ptr_array_index(data_arr,cntz);
                    data[ind_tup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tup;
            }
            for (cntx = 0; cntx < xres; ++cntx) {
                // Retrace Up
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz+zmax);
                    data[ind_retup] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                --ind_retup;
            }
            ind_tup -= 2*xres;
        }
        for (cnty = 0; cnty < yres; ++cnty) {
            for (cntx = 0; cntx < xres; ++cntx) {
                // Trace Down
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz+zmax*2);
                    data[ind_tdown] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                ++ind_tdown;
            }
            for (cntx = 0; cntx < xres; ++cntx) {
                // Retrace Down
                for(cntz = 0; cntz < zmax && n < navail; ++cntz){
                    data = g_ptr_array_index(data_arr,cntz+zmax*3);
                    data[ind_retdown] =  fac * (gwy_get_gint32_le(fp)-offset);
                    ++n;
                }
                --ind_retdown;
            }
            ind_retdown += 2*xres;
        }
    }

    // check if all data were read correctly
    if (n != navail) {
        err_FILE_TYPE(error, "Omicron Flat");
        g_ptr_array_free(data_arr,TRUE);
        g_ptr_array_free(dfield_arr,TRUE);
        return;
    }

    gwy_debug("CITS data successfully read");

    zmax = zmax/(zmirrored+1); // actuall number of point in spectroscopy curves

    for (i = 0; i < zmax; ++i) {
        dfield = g_ptr_array_index(dfield_arr,i);
        g_snprintf(key, sizeof(key), "/%i/data", i);
        gwy_container_set_object_by_name(container, key, dfield);

        g_snprintf(val, sizeof(val), "Trace Up, %3.5g V", zstart+zinc*i);
        g_snprintf(key, sizeof(key), "/%i/data/title", i);
        gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
    }
    m = 1;
    if (zmirrored) {
        n = zmax;
        for (i = zmax; i < zmax*2; ++i) {
            dfield = g_ptr_array_index(dfield_arr,i);
            g_snprintf(key, sizeof(key), "/%i/data", i);
            gwy_container_set_object_by_name(container, key, dfield);

            g_snprintf(val, sizeof(val), "Trace Up Mirrored, %3.5g V", zstart+zinc*n--);
            g_snprintf(key, sizeof(key), "/%i/data/title", i);
            gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
        }
        ++m;
    }
    if (xspecmir) {
        n = 0;
        for (i = zmax*m; i < zmax * (m+1); ++i) {
            dfield = g_ptr_array_index(dfield_arr,i);
            g_snprintf(key, sizeof(key), "/%i/data", i);
            gwy_container_set_object_by_name(container, key, dfield);

            g_snprintf(val, sizeof(val), "reTrace Up, %3.5g V", zstart+zinc*n++);
            g_snprintf(key, sizeof(key), "/%i/data/title", i);
            gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
        }
        ++m;
        if (zmirrored) {
            n = zmax;
            for (i = zmax*m; i < zmax * (m+1); ++i) {
                dfield = g_ptr_array_index(dfield_arr,i);
                g_snprintf(key, sizeof(key), "/%i/data", i);
                gwy_container_set_object_by_name(container, key, dfield);

                g_snprintf(val, sizeof(val), "reTrace Up Mirrored, %3.5g V", zstart+zinc*n--);
                g_snprintf(key, sizeof(key), "/%i/data/title", i);
                gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
            }
            ++m;
        }
    }
    if (yspecmir) {
        n = 0;
        for (i = zmax*m; i < zmax * (m+1); ++i) {
            dfield = g_ptr_array_index(dfield_arr,i);
            g_snprintf(key, sizeof(key), "/%i/data", i);
            gwy_container_set_object_by_name(container, key, dfield);

            g_snprintf(val, sizeof(val), "Trace Down, %3.5g V", zstart+zinc*n++);
            g_snprintf(key, sizeof(key), "/%i/data/title", i);
            gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
        }
        ++m;
        if (zmirrored) {
            n = zmax;
            for (i = zmax*m; i < zmax * (m+1); ++i) {
                dfield = g_ptr_array_index(dfield_arr,i);
                g_snprintf(key, sizeof(key), "/%i/data", i);
                gwy_container_set_object_by_name(container, key, dfield);

                g_snprintf(val, sizeof(val), "Trace Down Mirrored, %3.5g V", zstart+zinc*n--);
                g_snprintf(key, sizeof(key), "/%i/data/title", i);
                gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
            }
            ++m;
        }
        if (xspecmir) {
            n = 0;
            for (i = zmax*m; i < zmax * (m+1); ++i) {
                dfield = g_ptr_array_index(dfield_arr,i);
                g_snprintf(key, sizeof(key), "/%i/data", i);
                gwy_container_set_object_by_name(container, key, dfield);

                g_snprintf(val, sizeof(val), "reTrace Down, %3.5g V", zstart+zinc*n++);
                g_snprintf(key, sizeof(key), "/%i/data/title", i);
                gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
            }
            ++m;
            if (zmirrored) {
                n = zmax;
                for (i = zmax*m; i < zmax * (m+1); ++i) {
                    dfield = g_ptr_array_index(dfield_arr,i);
                    g_snprintf(key, sizeof(key), "/%i/data", i);
                    gwy_container_set_object_by_name(container, key, dfield);

                    g_snprintf(val, sizeof(val), "reTrace Down Mirrored, %3.5g V", zstart+zinc*n--);
                    g_snprintf(key, sizeof(key), "/%i/data/title", i);
                    gwy_container_set_string_by_name(container, key,(guchar*)g_strdup(val));
                }
                ++m;
            }
        }
    }

    gwy_debug("CITS inserted to container successfully.");

    g_ptr_array_free(data_arr,TRUE);
    g_ptr_array_free(dfield_arr,TRUE);

    return;
}

/**
 * omicronflat_readsps:
 *
 * Reads the data from the file buffer and store it with the correct shape
 * as SPS into the data container.
 *
 **/
static void
omicronflat_readsps(GwyContainer *container, GwyContainer *metainfo,
                    const guchar **fp, const gsize fp_end, GError **error)
{
    guint32 i, j, n, navail, nexpect;
    gchar *t;
    GQuark quark;
    gboolean mirrored;
    gdouble length, width;
    gdouble fac = 1.;
    gdouble offset = 0;
    gdouble spec_offset = 0;

    GwySpectra *spectra = NULL;
    const guchar* sunit;
    GwySIUnit *unitx = NULL;
    GwySIUnit *unity = NULL;
    GwyDataLine *dline;
    gdouble *data;

    gwy_debug("File is spectroscopy curves (SPS) ");

    nexpect = gwy_container_get_int32_by_name(metainfo,"brickletSize");
    navail = gwy_container_get_int32_by_name(metainfo,"dataItemSize");

    if (navail > nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    // Check buffer length for data available
    if (fp_end < (gsize)*fp + navail*4) { // navail = number of point, each gint32
        err_TOO_SHORT(error);
        return;
    }

    mirrored = gwy_container_get_boolean_by_name(metainfo,
                                                "/axis/0/mirrored");
    length = gwy_container_get_int32_by_name(metainfo,
                                                "/axis/0/clockCount")
             / (mirrored+1);
    width = gwy_container_get_double_by_name(metainfo,
                                                "/axis/0/incrementPhysical")
            * length;
    spec_offset = gwy_container_get_double_by_name(metainfo,
                                            "/axis/0/startValuePhysical");

    // check if data shape was understood correctly
    if (length * (mirrored+1) != nexpect) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    sunit = gwy_container_get_string_by_name(metainfo, "/axis/0/units");
    unitx = gwy_si_unit_new((gchar*)sunit);

    sunit = gwy_container_get_string_by_name(metainfo, "/channel/units");
    unity = gwy_si_unit_new((gchar*)sunit);

    omicronflat_getscalingfactor(metainfo, &fac, &offset);
    spectra = gwy_spectra_new();
    n = 0;

    // Do it twice for mirrored spectroscopy
    // FIXME check if the scale is handled correctly for mirrored SPS curves
    for (i = 0; i < (mirrored+1); ++i) {

        dline = gwy_data_line_new(length, width, TRUE);
        gwy_data_line_set_si_unit_x(dline, unitx);
        gwy_data_line_set_si_unit_y(dline, unity);
        gwy_data_line_set_offset(dline,spec_offset);
        data = gwy_data_line_get_data(dline);

        for (j = 0; j < length && n < navail; ++j) {
            data[j] = fac * (gwy_get_gint32_le(fp)-offset);
            ++n;
        }

        gwy_spectra_add_spectrum(spectra, dline, 0, 0);
        // FIXME insert correct position from offset information read below
        // FIXME set the correct xy unit gwy_spectra_set_unit_xy

        g_object_unref(dline);
    }

    // check if all data were read correctly
    if (n != navail) {
        err_FILE_TYPE(error, "Omicron Flat");
        return;
    }

    gwy_debug("SPS data successfully read");

    // FIXME is there any way to add directly with a name not using a quark ?
    //   gwy_container_set_object_by_name(container, "/0/spectrum", spectra);
    //   even so it does not appear in the data browser
    t = g_strconcat("Spectroscopy", NULL); // FIXME set a proper title
    gwy_spectra_set_title(spectra, t);
    quark = gwy_app_get_spectra_key_for_id(0);
    gwy_container_set_object(container, quark, spectra);

    g_free(t);
    g_object_unref(unitx);
    g_object_unref(unity);
    g_object_unref(spectra);

    return;
}

// TODO : For these cases we need some example files
#ifdef omicronflat_todo
static void
omicronflat_readforcedist(GwyContainer *container, GwyContainer *metainfo,
                          const guchar **fp, const gsize fp_end, GError **error)
{
    return;
}

static void
omicronflat_readtimevarying(GwyContainer *container, GwyContainer *metainfo,
                            const guchar **fp, const gsize fp_end, GError **error)
{
    return;
}
#endif

/**
 * omicronflat_load:
 *
 * Handles and read omicron flat file format
 *
 * Returns : A GwyContainer with the measured data and experiment information
 *
 **/
static GwyContainer*
omicronflat_load(const gchar *filename, G_GNUC_UNUSED GwyRunType mode, GError **error)
{
    GError *tmperr = NULL;

    GwyContainer *metainfo = NULL; // store meta informations about the data structure
    GwyContainer *metadata = NULL; // store meta informations about the measurements
    GwyContainer *container = NULL;

    guchar* file_buffer = NULL;
    const guchar* fp = NULL;
    gsize file_buffer_size;
    gsize fp_end;
    GError *err = NULL;

    // Try to get file content
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
    container = gwy_container_new();

    // Get metainfo about the file structure
    omicronflat_readmetainfo(metainfo, &fp, fp_end, &tmperr);
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }

    // Try to figure out the file contents from the dataView value returned
    // See Omicron Vernissage MATRIX Result File Access and Export manual p.20

    // If the file contains "topography images, current images or similar 2D data"
    if (IS_2DIMAGE)
        omicronflat_read2dimage(container, metainfo, &fp, fp_end, &tmperr);
    // Else if the file contains "planes form a volume CITS data cube"
    else if (IS_CITS)
        omicronflat_readcits(container, metainfo, &fp, fp_end, &tmperr);
    // Else if the file contains "spectroscopy curves (SPS)"
    else if (IS_SPS)
        omicronflat_readsps(container, metainfo, &fp, fp_end, &tmperr);
    // Else if the file contains "Force/distance curves" (1D data)
    else if (IS_FORCE_DIST){
        // TODO cf. above
        // omicronflat_readforcedist(container, metainfo, &fp, fp_end, &tmperr);
        gwy_debug("File is force/distance curves ");
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Force-distance curve "
                        "are not supported."));
    }
    // Else if the file contains "Temporally varying signal acquired over time"
    else if (IS_TIME_VARYING) {
        // TODO cf. above
        // omicronflat_readtimevarying(container, metainfo, &fp, fp_end, &tmperr);
        gwy_debug("File is Temporally varying signal acquired over time ");
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Temporally varying signal "
                        "is not supported."));
        goto fail;
    }
    else {
        err_FILE_TYPE(error, "Omicron Flat");
        goto fail;
    }

    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }

    omicronflat_readmetadata(metadata, &fp, fp_end, &tmperr);
    if (tmperr != NULL) {
        g_propagate_error(error, tmperr);
        goto fail;
    }

#ifdef DEBUG
    if (fp_end == (gsize)fp)
        gwy_debug("File read, no data or information left.");
    else
        gwy_debug("Not all the data in the file was read.");
#endif

    // copy usefull metainfo in metadata
    gwy_container_set_string_by_name(metadata,"Info:creation_time",
            (guchar*)g_strdup(gwy_container_get_string_by_name(metainfo,"creation_time")));
    gwy_container_set_string_by_name(metadata,"Info:comment",
            (guchar*)g_strdup(gwy_container_get_string_by_name(metainfo,"comment")));

    gwy_container_set_object_by_name(container, "/0/meta", metadata);

    g_object_unref(metadata);
    g_object_unref(metainfo);
    gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);

    return container;

fail:
    gwy_debug("The file is either corrupted, or has an unknown/unhandled format. Module failed to read the file, you can blame the programmer… or help him…");

    g_object_unref(metainfo);
    g_object_unref(metadata);
    g_object_unref(container);
    gwy_file_abandon_contents(file_buffer, file_buffer_size, NULL);

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
