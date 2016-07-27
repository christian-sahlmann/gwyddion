/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 * <mime-type type="application/x-dektak-xml">
 *   <comment>Dektak XML profilometry data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="&lt;?xml">
 *       <match type="string" offset="20:60" value="&lt;DataContainer typeid=\"125\""/>
 *     </match>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # SIS
 * 0 string \x3c?xml
 * >&0 search/80 \x3cDataContainer\ typeid="125" Dektak XML profilometry data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Dektak XML profilometry data
 * .xml
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#ifdef HAVE_MEMRCHR
#define strlenrchr(s,c,len) (gchar*)memrchr((s),(c),(len))
#else
#define strlenrchr(s,c,len) strrchr((s),(c))
#endif

#define MAGIC "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define MEAS_SETTINGS "/DataContainer/MetaData/MeasurementSettings"
#define RAW_1D_DATA   "/DataContainer/1D_Data/Raw"
#define Micrometre 1e-6

/* This is what I guessed from observing the XML.  At this moment we ignore
 * the type ids anyway. */
typedef enum {
    DEKTAKXML_BOOLEAN      = 1,   /* Takes value 0 and 1 */
    DEKTAKXML_COUNT        = 11,  /* Positive integer which is an item count */
    DEKTAKXML_DOUBLE       = 12,  /* Any number */
    DEKTAKXML_INT          = 13,  /* Arbitrary integer */
    DEKTAKXML_TYPE_ID      = 14,  /* Type id, some kind of enum? */
    DEKTAKXML_STRING       = 18,  /* Free-form string value */
    DEKTAKXML_VALUE_UNIT   = 19,  /* Have Value and Unit children */
    DEKTAKXML_TIME_STAMP   = 21,  /* Datetime (formatted as string) */
    DEKTAKXML_BASE64       = 64,  /* Base64-encoded raw data array */
    DEKTAKXML_STRING_LIST  = 66,  /* List of Str */
    DEKTAKXML_RAW_DATA     = 70,  /* Parent/wrapper tag of raw data */
    DEKTAKXML_POS_RAW_DATA = 124, /* Base64-encoded positions, not sure how
                                     it differs from 64 */
    DEKTAKXML_CONTAINER    = 125, /* General nested data structure */
} DektakXMLTypeID;

typedef struct {
    gchar *name;
    gsize len;
    guchar *data;
} DektakXMLRawData;

typedef struct {
    GHashTable *hash;
    GString *path;
    GPtrArray *channels;
    GArray *rawdata;
} DektakXMLFile;

static gboolean      module_register (void);
static gint          dektakxml_detect(const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* dektakxml_load  (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gdouble*      convert_raw_data(const DektakXMLRawData *rawdata,
                                      guint res,
                                      gdouble q,
                                      GError **error);
static void          dektakxml_init  (DektakXMLFile *dxfile);
static void          dektakxml_free  (DektakXMLFile *dxfile);
static void          start_element   (GMarkupParseContext *context,
                                      const gchar *element_name,
                                      const gchar **attribute_names,
                                      const gchar **attribute_values,
                                      gpointer user_data,
                                      GError **error);
static void          end_element     (GMarkupParseContext *context,
                                      const gchar *element_name,
                                      gpointer user_data,
                                      GError **error);
static void          text            (GMarkupParseContext *context,
                                      const gchar *value,
                                      gsize value_len,
                                      gpointer user_data,
                                      GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Dektak XML data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("dektakxml",
                           N_("Dektak XML data files (.xml)"),
                           (GwyFileDetectFunc)&dektakxml_detect,
                           (GwyFileLoadFunc)&dektakxml_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
dektakxml_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    const gchar *head = fileinfo->head;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || (memcmp(head, MAGIC, MAGIC_SIZE) != 0))
        return 0;

    /* Look for some things that should be present after the general XML
     * header. */
    gwy_debug("magic OK");
    head += MAGIC_SIZE;
    while (g_ascii_isspace(*head))
        head++;

    if (!g_str_has_prefix(head, "<DataContainer typeid=\"125\""))
        return 0;

    gwy_debug("DataContainer tag found");
    head += strlen("<DataContainer typeid=\"125\"");
    if (!strstr(head, " key=\"MeasurementSettings\""))
        return 0;

    return 85;
}

static GwyContainer*
dektakxml_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *container = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GMarkupParser parser = { start_element, end_element, text, NULL, NULL };
    GMarkupParseContext *context = NULL;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    DektakXMLFile dxfile;
    const gchar *s, *title;
    GwySIUnit *xunit, *yunit;
    gint power10;
    guint i, res;
    gdouble real, qy;
    gdouble *xdata = NULL, *ydata = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Dektak XML");
        goto fail;
    }

    dektakxml_init(&dxfile);
    context = g_markup_parse_context_new(&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                                         &dxfile, NULL);
    if (!g_markup_parse_context_parse(context, buffer, size, &err)
        || !g_markup_parse_context_end_parse(context, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("XML parsing failed: %s"), err->message);
        g_clear_error(&err);
        goto fail;
    }

    if (!dxfile.rawdata->len) {
        err_NO_DATA(error);
        goto fail;
    }

    /* Many things have two values, one in measurement settings and the other
     * in the data.  The value in data seems to be the actual one, the value
     * in settings is nominal? */
    if (!require_keys(dxfile.hash, error,
                      RAW_1D_DATA "/NumPoints",
                      RAW_1D_DATA "/Extent/Value",
                      RAW_1D_DATA "/Extent/Unit",
                      RAW_1D_DATA "/DataScale/Value",
                      RAW_1D_DATA "/DataScale/Unit",
                      NULL))
        goto fail;

    s = g_hash_table_lookup(dxfile.hash, MEAS_SETTINGS "/SamplesToLog");
    res = atoi(s);

    s = g_hash_table_lookup(dxfile.hash, MEAS_SETTINGS "/ScanLength/Value");
    real = g_ascii_strtod(s, NULL);

    s = g_hash_table_lookup(dxfile.hash, MEAS_SETTINGS "/ScanLength/Unit");
    xunit = gwy_si_unit_new_parse(s, &power10);
    real *= pow10(power10);

    s = g_hash_table_lookup(dxfile.hash, RAW_1D_DATA "/DataScale/Value");
    qy = g_ascii_strtod(s, NULL);

    s = g_hash_table_lookup(dxfile.hash, RAW_1D_DATA "/DataScale/Unit");
    yunit = gwy_si_unit_new_parse(s, &power10);
    qy *= pow10(power10);

    if (!(title = g_hash_table_lookup(dxfile.hash,
                                      "/DataContainer/MetaData/DataKind")))
        title = "Curve";

    gmodel = gwy_graph_model_new();
    for (i = 0; i < dxfile.rawdata->len; i++) {
        DektakXMLRawData *rawdata = &g_array_index(dxfile.rawdata,
                                                   DektakXMLRawData, i);
        if (g_str_has_suffix(rawdata->name, "/PositionFunction")) {
            if (!(xdata = convert_raw_data(rawdata, res, Micrometre, error)))
                goto fail;
        }
    }
    if (!xdata) {
        xdata = g_new(gdouble, res);
        for (i = 0; i < res; i++)
            xdata[i] = real*i/(res - 1.0);
    }

    for (i = 0; i < dxfile.rawdata->len; i++) {
        DektakXMLRawData *rawdata = &g_array_index(dxfile.rawdata,
                                                   DektakXMLRawData, i);
        if (g_str_has_suffix(rawdata->name, "/PositionFunction"))
            continue;

        if (!(ydata = convert_raw_data(rawdata, res, qy, error)))
            goto fail;

        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(i),
                     "description", title,
                     NULL);
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_free(ydata);
    }

    if (gwy_graph_model_get_n_curves(gmodel)) {
        g_object_set(gmodel,
                     "si-unit-x", xunit,
                     "si-unit-y", yunit,
                     "title", title,
                     NULL);
        container = gwy_container_new();
        gwy_container_set_object(container,
                                 gwy_app_get_graph_key_for_id(0), gmodel);
    }
    g_object_unref(gmodel);
    g_object_unref(xunit);
    g_object_unref(yunit);

fail:
    dektakxml_free(&dxfile);
    g_free(xdata);
    g_free(buffer);

    return container;
}

static gdouble*
convert_raw_data(const DektakXMLRawData *rawdata,
                 guint res, gdouble q, GError **error)
{
    gsize expected_size = res*sizeof(gdouble);
    gsize size = rawdata->len;
    gdouble *d;

    if (err_SIZE_MISMATCH(error, expected_size, size, FALSE))
        return NULL;

    d = g_new(gdouble, res);
    gwy_convert_raw_data(rawdata->data + (size - expected_size), res, 1,
                         GWY_RAW_DATA_DOUBLE, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         d, q, 0.0);
    return d;
}

static void
dektakxml_init(DektakXMLFile *dxfile)
{
    gwy_clear(dxfile, 1);

    dxfile->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
    dxfile->path = g_string_new(NULL);
    dxfile->channels = g_ptr_array_new();
    dxfile->rawdata = g_array_new(FALSE, FALSE, sizeof(DektakXMLRawData));
}

static void
dektakxml_free(DektakXMLFile *dxfile)
{
    guint i;

    if (dxfile->hash)
        g_hash_table_destroy(dxfile->hash);
    if (dxfile->path)
        g_string_free(dxfile->path, TRUE);

    if (dxfile->channels) {
        for (i = 0; i < dxfile->channels->len; i++)
            g_free(g_ptr_array_index(dxfile->channels, i));
        g_ptr_array_free(dxfile->channels, TRUE);
    }

    if (dxfile->rawdata) {
        for (i = 0; i < dxfile->rawdata->len; i++) {
            DektakXMLRawData *rawdata = &g_array_index(dxfile->rawdata,
                                                      DektakXMLRawData, i);
            g_free(rawdata->name);
            g_free(rawdata->data);
        }
        g_array_free(dxfile->rawdata, TRUE);
    }
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              const gchar **attribute_names,
              const gchar **attribute_values,
              gpointer user_data,
              GError **error)
{
    DektakXMLFile *dxfile = (DektakXMLFile*)user_data;
    guint i;

    gwy_debug("<%s>", element_name);
    if (!dxfile->path->len && !gwy_strequal(element_name, "DataContainer")) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                    _("Top-level element is not ‘%s’."), "OME");
        return;
    }

    g_string_append_c(dxfile->path, '/');
    for (i = 0; attribute_names[i]; i++) {
        if (gwy_strequal(attribute_names[i], "key")) {
            g_string_append(dxfile->path, attribute_values[i]);
            return;
        }
    }
    g_string_append(dxfile->path, element_name);
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            G_GNUC_UNUSED const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    DektakXMLFile *dxfile = (DektakXMLFile*)user_data;
    gchar *pos;

    gwy_debug("</%s>", element_name);
    pos = strlenrchr(dxfile->path->str, '/', dxfile->path->len);
    g_string_truncate(dxfile->path, pos - dxfile->path->str);
}

static void
text(G_GNUC_UNUSED GMarkupParseContext *context,
     const gchar *value,
     gsize value_len,
     gpointer user_data,
     G_GNUC_UNUSED GError **error)
{
    DektakXMLFile *dxfile = (DektakXMLFile*)user_data;
    const gchar *path = dxfile->path->str;

    gwy_debug("%s (%lu)", path, (gulong)value_len);
    if (!value_len)
        return;

    if (gwy_stramong(path,
                     "/DataContainer/1D_Data/Raw/Array",
                     "/DataContainer/1D_Data/Raw/PositionFunction",
                     NULL)) {
        DektakXMLRawData rawdata;

        /* XXX: This is not the actual double data array.  There are some
         * bytes before, apparently 5 for data and more for the position
         * function. */
        rawdata.data = g_base64_decode(value, &rawdata.len);
        if (rawdata.len) {
            rawdata.name = g_strdup(path);
            g_array_append_val(dxfile->rawdata, rawdata);
            gwy_debug("raw data <%s> of decoded length %lu",
                      path, (gulong)rawdata.len);
        }
        else {
            /* The parser returns some whitespace around the actual data
             * as separate text chunks. */
            g_free(rawdata.data);
        }
        return;
    }

    /* XXX: Handle the channels array.  But how?  What is actually the
     * general structure and what is the channel title?  */
    g_hash_table_insert(dxfile->hash, g_strdup(path), g_strdup(value));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
