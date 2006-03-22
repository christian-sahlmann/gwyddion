/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <limits.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

/* Fix netcdf.h using _MIPS_SZLONG unconditionally.
 * On MIPS (hypotetically) we get _MIPS_SZLONG from limits.h, otherwise
 * the value does not matter. */
#ifndef _MIPS_SZLONG
#define _MIPS_SZLONG 4
#endif

#include <mfhdf.h>

#include "get.h"
#include "err.h"

#define MAGIC "\x0e\x03\x13\x01"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".hdf"

static gboolean      module_register(void);
static gint          hdf_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* hdf_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Hierarchical Data Format (HDF) files, version 4."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("hdf4file",
                           N_("Hierarchical Data Format v4 (.hdf)"),
                           (GwyFileDetectFunc)&hdf_detect,
                           (GwyFileLoadFunc)&hdf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
hdf_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Whatver Hishdf() does, we check magic header ourselves because it
     * weeds out non-HDF files very quickly, namely no fopen() is needed. */
    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return Hishdf(fileinfo->name) ? 90 : 0;

    return 0;
}

static inline void
err_HDF(GError **error,
        const gchar *funcname)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("HDF library function %s failed with error: %s."),
                funcname, HEstring(HEvalue(1)));
}

#ifdef DEBUG
static struct {
    int32 id;
    const gchar *desc;
}
const data_types[] = {
    { DFNT_CHAR8,   "signed 8-bit character"   },
    { DFNT_UCHAR8,  "unsigned 8-bit character" },
    { DFNT_INT8,    "signed 8-bit integer"     },
    { DFNT_UINT8,   "unsigned 8-bit integer"   },
    { DFNT_INT16,   "signed 16-bit integer"    },
    { DFNT_UINT16,  "unsigned 16-bit integer"  },
    { DFNT_INT32,   "signed 32-bit integer"    },
    { DFNT_UINT32,  "unsigned 32-bit integer"  },
    { DFNT_FLOAT32, "single precision float"   },
    { DFNT_FLOAT64, "double precision float"   },
};

static gchar*
describe_data_type(int32 id)
{
    guint i;
    int32 baseid;

    baseid = id & ~(DFNT_NATIVE | DFNT_LITEND);
    for (i = 0; i < sizeof(data_types)/sizeof(data_types[0]); i++) {
        if (baseid == data_types[i].id)
            return g_strdup_printf("%s%s%s",
                                   (id & DFNT_NATIVE) ? "native " : "",
                                   (id & DFNT_LITEND) ? "little endian " : "",
                                   data_types[i].desc);
    }

    return g_strdup("UNKNOWN");
}
#endif

static guint
get_data_type_size(int32 id,
                   GError **error)
{
    /* Native data format is Evil. */
    if (id & DFNT_NATIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    N_("HDF data type is `native', in other words only the "
                       "creator knows what the data type is.  Such a data "
                       "cannot be reliably imported to other applications, "
                       "please urge on the creator of this file to write "
                       "portable HDF files."));
        return 0;
    }

    switch (id & ~(DFNT_NATIVE | DFNT_LITEND)) {
        case DFNT_CHAR8:
        case DFNT_UCHAR8:
        case DFNT_INT8:
        case DFNT_UINT8:
        return 1;
        break;

        case DFNT_INT16:
        case DFNT_UINT16:
        return 2;
        break;

        case DFNT_INT32:
        case DFNT_UINT32:
        case DFNT_FLOAT32:
        return 4;
        break;

        case DFNT_FLOAT64:
        return 8;
        break;

        default:
        err_UNSUPPORTED(error, "data_type");
        return 0;
        break;
    }
}

static GwyDataField*
read_data_field(int32 sd_id,
                int32 sds_id,
                int32 data_type,
                int32 *dim_sizes,
                GError **error)
{
    gchar label[MAX_NC_NAME], unit[MAX_NC_NAME];
    GwyDataField *dfield;
    int32 start[2], edges[2];
    guint data_size;
    guchar *d;
    gint i, xres, yres;
    gdouble *data;
    intn status;

    if ((data_size = get_data_type_size(data_type, error)) == 0)
        return NULL;

    status = SDgetdatastrs(sds_id, label, unit, NULL, NULL, MAX_NC_NAME);
    gwy_debug("label: `%s'", label);
    gwy_debug("unit: `%s'", unit);

    start[0] = start[1] = 0;
    yres = edges[0] = dim_sizes[0];
    xres = edges[1] = dim_sizes[1];
    d = g_malloc(edges[0] * edges[1] * data_size);
    if ((status = SDreaddata(sds_id, start, NULL, edges, d)) == FAIL) {
        err_HDF(error, "SDreaddata");
        g_free(d);
        return NULL;
    }

    dfield = gwy_data_field_new(xres, yres, 1.0, 1.0, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (data_type) {
        case DFNT_CHAR8:
        case DFNT_CHAR8 | DFNT_LITEND:
        case DFNT_INT8:
        case DFNT_INT8 | DFNT_LITEND:
        {
            gint8 *d8 = (gint8*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = d8[i];
        }
        break;

        case DFNT_UCHAR8:
        case DFNT_UCHAR8 | DFNT_LITEND:
        case DFNT_UINT8:
        case DFNT_UINT8 | DFNT_LITEND:
        {
            guint8 *d8 = (guint8*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = d8[i];
        }
        break;

        case DFNT_INT16:
        {
            gint16 *d16 = (gint16*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_BE(d16[i]);
        }
        break;

        case DFNT_INT16 | DFNT_LITEND:
        {
            gint16 *d16 = (gint16*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_LE(d16[i]);
        }
        break;

        case DFNT_UINT16:
        {
            gint16 *d16 = (gint16*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GUINT16_FROM_BE(d16[i]);
        }
        break;

        case DFNT_UINT16 | DFNT_LITEND:
        {
            gint16 *d16 = (gint16*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GUINT16_FROM_LE(d16[i]);
        }
        break;

        case DFNT_INT32:
        {
            gint32 *d32 = (gint32*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT32_FROM_BE(d32[i]);
        }
        break;

        case DFNT_INT32 | DFNT_LITEND:
        {
            gint32 *d32 = (gint32*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT32_FROM_LE(d32[i]);
        }
        break;

        case DFNT_UINT32:
        {
            gint32 *d32 = (gint32*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GUINT32_FROM_BE(d32[i]);
        }
        break;

        case DFNT_UINT32 | DFNT_LITEND:
        {
            gint32 *d32 = (gint32*)d;

            for (i = 0; i < xres*yres; i++)
                data[i] = GUINT32_FROM_LE(d32[i]);
        }
        break;

        case DFNT_FLOAT32:
        {
            const guchar *p = d;

            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT_BE(&p);
        }
        break;

        case DFNT_FLOAT32 | DFNT_LITEND:
        {
            const guchar *p = d;

            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT(&p);
        }
        break;

        case DFNT_FLOAT64:
        {
            const guchar *p = d;

            for (i = 0; i < xres*yres; i++)
                data[i] = get_DOUBLE_BE(&p);
        }
        break;

        case DFNT_FLOAT64 | DFNT_LITEND:
        {
            const guchar *p = d;

            for (i = 0; i < xres*yres; i++)
                data[i] = get_DOUBLE(&p);
        }
        break;

        default:
        err_UNSUPPORTED(error, "data_type");
        gwy_object_unref(dfield);
        g_free(d);
        return NULL;
    }

    g_free(d);

    return dfield;
}

static GwyContainer*
hdf_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL;
    int32 sd_id, sds_index, sds_id;
    int32 n_datasets, n_file_attrs;
    int32 dim_sizes[MAX_VAR_DIMS];
    int32 rank, data_type, n_attrs;
    char name[MAX_NC_NAME+1];
    gchar key[24];
    intn status;
    guint i, ndata;
    gchar *s;

    if (!Hishdf(filename)) {
        err_FILE_TYPE(error, "HDF");
        return NULL;
    }

    if ((sd_id = SDstart(filename, DFACC_READ)) == FAIL) {
        err_HDF(error, "SDstart");
        return NULL;
    }

    if ((status = SDfileinfo(sd_id, &n_datasets, &n_file_attrs)) == FAIL) {
        err_HDF(error, "SDfileinfo");
        SDend(sd_id);
        return NULL;
    }
    gwy_debug("n_datasets: %u, n_file_attrs = %u",
              (guint)n_datasets, (guint)n_file_attrs);

    name[MAX_NC_NAME] = '\0';
    ndata = 0;
    container = gwy_container_new();

    for (sds_index = 0; sds_index < n_datasets; sds_index++) {
        gwy_debug("examining data set #%u", (guint)sds_index);
        if ((sds_id = SDselect(sd_id, sds_index)) == FAIL) {
            err_HDF(error, "SDselect");
            SDend(sd_id);
            gwy_object_unref(container);
            return NULL;
        }

        if (SDiscoordvar(sds_id)) {
            gwy_debug("array is a dimension scale, skipping");
            SDend(sds_id);
            continue;
        }

        if ((status = SDgetinfo(sds_id, name, &rank, dim_sizes,
                                &data_type, &n_attrs)) == FAIL) {
            err_HDF(error, "SDgetinfo");
            SDendaccess(sds_id);
            SDend(sd_id);
            gwy_object_unref(container);
            return NULL;
        }
        gwy_debug("name: `%s', rank: %u, n_attrs: %u",
                  name, (guint)rank, (guint)n_attrs);
#ifdef DEBUG
        s = describe_data_type(data_type);
        gwy_debug("data_type: %u (%s)", (guint)data_type, s);
        g_free(s);
        {
            GString *str;

            str = g_string_new("");
            for (i = 0; i < rank; i++)
                g_string_append_printf(str, " %u", (guint)dim_sizes[i]);
            gwy_debug("dim_sizes:%s", str->str);
            g_string_free(str, TRUE);
        }
#endif
        if (rank > 2) {
            gwy_debug("ignoring data set with rank > 2");
        }
        else if (rank < 2) {
            gwy_debug("ignoring data set with rank < 2, FIXME!");
        }
        else {
            dfield = read_data_field(sd_id, sds_id, data_type, dim_sizes,
                                     error);
            if (!dfield) {
                SDendaccess(sds_id);
                SDend(sd_id);
                gwy_object_unref(container);
                return NULL;
            }

            g_snprintf(key, sizeof(key), "/%u/data", ndata);
            gwy_container_set_object_by_name(container, key, dfield);
            g_object_unref(dfield);
        }

        if ((status = SDendaccess(sds_id)) == FAIL) {
            err_HDF(error, "SDendaccess");
            SDend(sd_id);
            gwy_object_unref(container);
            return NULL;
        }
    }

    if ((status = SDend(sd_id)) == FAIL) {
        err_HDF(error, "SDend");
        gwy_object_unref(container);
        return NULL;
    }

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

