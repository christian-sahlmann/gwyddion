/*
 *  $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
 * [FILE-MAGIC-USERGUIDE]
 * OpenGPS X3P (ISO 5436-2)
 * .x3p
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unzip.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#ifdef HAVE_MEMRCHR
#define strlenrchr(s,c,len) (gchar*)memrchr((s),(c),(len))
#else
#define strlenrchr(s,c,len) strrchr((s),(c))
#endif

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC1 "main.xml"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 "bindata/"
#define MAGIC2_SIZE (sizeof(MAGIC2)-1)
#define BLOODY_UTF8_BOM "\xef\xbb\xbf"
#define EXTENSION ".x3p"

#define MAT_DIM_PREFIX "/ISO5436_2/Record3/MatrixDimension"
#define AXES_PREFIX "/ISO5436_2/Record1/Axes"
#define DATA_LINK_PREFIX "/ISO5436_2/Record3/DataLink"

typedef enum {
    X3P_FEATURE_SUR,
    X3P_FEATURE_PRF,
    X3P_FEATURE_PCL,
} X3PFeatureType;

typedef struct {
    GHashTable *hash;
    GString *path;
    X3PFeatureType feature_type;
    gboolean seen_datum;
    guint xres;
    guint yres;
    guint zres;
    guint ndata;
    guint datapos;
    gdouble dx;
    gdouble dy;
    gdouble dz;
    gdouble xoff;
    gdouble yoff;
    gdouble zoff;
    gdouble *values;
    gboolean *valid;
} X3PFile;

static gboolean      module_register       (void);
static gint          x3p_detect            (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* x3p_load              (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static void          create_images         (const X3PFile *x3pfile,
                                            GwyContainer *container);
static void          create_profiles       (const X3PFile *x3pfile,
                                            GwyContainer *container);
static gboolean      x3p_parse_main        (unzFile *zipfile,
                                            X3PFile *x3pfile,
                                            GError **error);
static gboolean      data_start            (X3PFile *x3pfile,
                                            GError **error);
static gboolean      read_binary_data      (X3PFile *x3pfile,
                                            unzFile *zipfile,
                                            GError **error);
static GwyContainer* get_meta              (const X3PFile *x3pfile);
static void          add_meta_record       (gpointer hkey,
                                            gpointer hvalue,
                                            gpointer user_data);
static guchar*       x3p_get_file_content  (unzFile *zipfile,
                                            gsize *contentsize,
                                            GError **error);
static void          x3p_file_free         (X3PFile *x3pfile);
static gboolean      x3p_file_get_data_type(const gchar *type,
                                            GwyRawDataType *rawtype,
                                            GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads OpenGPS .x3p files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("opengps",
                           N_("OpenGPS data (.x3p)"),
                           (GwyFileDetectFunc)&x3p_detect,
                           (GwyFileLoadFunc)&x3p_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
x3p_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    unzFile zipfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Generic ZIP file. */
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It contains main.xml and maybe directory bindata.  One of them should be
     * somewehre near the begining of the file. */
    if (!gwy_memmem(fileinfo->head, fileinfo->buffer_len,
                    MAGIC1, MAGIC1_SIZE)
        && !gwy_memmem(fileinfo->head, fileinfo->buffer_len,
                       MAGIC2, MAGIC2_SIZE))
        return 0;

    /* We have to realy look inside. */
    if (!(zipfile = unzOpen(fileinfo->name)))
        return 0;

    if (unzLocateFile(zipfile, "main.xml", 1) != UNZ_OK) {
        unzClose(zipfile);
        return 0;
    }

    unzClose(zipfile);

    return 100;
}

static GwyContainer*
x3p_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    X3PFile x3pfile;
    unzFile zipfile;

    zipfile = unzOpen(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    gwy_clear(&x3pfile, 1);
    if (!x3p_parse_main(zipfile, &x3pfile, error))
        goto fail;

    if (!x3pfile.ndata) {
        err_NO_DATA(error);
        goto fail;
    }

    if (!x3pfile.datapos) {
        if (!read_binary_data(&x3pfile, zipfile, error))
            goto fail;
    }

    container = gwy_container_new();
    if (x3pfile.feature_type == X3P_FEATURE_SUR)
        create_images(&x3pfile, container);
    else if (x3pfile.feature_type == X3P_FEATURE_PRF)
        create_profiles(&x3pfile, container);
    else {
        g_assert_not_reached();
    }

fail:
    gwy_debug("calling unzClose()");
    unzClose(zipfile);
    x3p_file_free(&x3pfile);

    return container;
}

static void
create_images(const X3PFile *x3pfile, GwyContainer *container)
{
    gint id;

    for (id = 0; id < x3pfile->zres; id++) {
        GwyContainer *meta;
        guint n = x3pfile->xres*x3pfile->yres, k;
        GwyDataField *dfield, *mask;
        const gboolean *valid = x3pfile->valid + id*n;
        GQuark quark;
        gchar buf[40];

        dfield = gwy_data_field_new(x3pfile->xres,
                                    x3pfile->yres,
                                    x3pfile->xres*x3pfile->dx,
                                    x3pfile->yres*x3pfile->dy,
                                    FALSE);
        memcpy(dfield->data, x3pfile->values + id*n, n*sizeof(gdouble));
        for (k = 0; k < n; k++) {
            if (!valid[k])
                dfield->data[k] = NAN;
        }

        quark = gwy_app_get_data_key_for_id(id);
        gwy_container_set_object(container, quark, dfield);

        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");
        gwy_app_channel_title_fall_back(container, id);
        gwy_app_channel_check_nonsquare(container, id);

        if ((mask = gwy_app_channel_mask_of_nans(dfield, TRUE))) {
            quark = gwy_app_get_mask_key_for_id(id);
            gwy_container_set_object(container, quark, mask);
            g_object_unref(mask);
        }
        g_object_unref(dfield);

        if ((meta = get_meta(x3pfile))) {
            g_snprintf(buf, sizeof(buf), "/%u/meta", id);
            gwy_container_set_object_by_name(container, buf, meta);
            g_object_unref(meta);
        }
    }
}

static void
create_profiles(const X3PFile *x3pfile,
                GwyContainer *container)
{
    GwyGraphModel *gmodel;
    GwySIUnit *siunitx, *siunity;
    GArray *validx, *validy;
    GQuark quark;
    gint id;

    gmodel = gwy_graph_model_new();
    siunitx = gwy_si_unit_new("m");
    siunity = gwy_si_unit_new("m");
    g_object_set(gmodel,
                 "title", "Profiles",
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 NULL);
    g_object_unref(siunity);
    g_object_unref(siunitx);

    validx = g_array_new(FALSE, FALSE, sizeof(gdouble));
    validy = g_array_new(FALSE, FALSE, sizeof(gdouble));
    for (id = 0; id < x3pfile->zres; id++) {
        guint n = x3pfile->xres;
        GwyGraphCurveModel *gcmodel;
        gchar *title;
        guint j;

        g_array_set_size(validx, 0);
        g_array_set_size(validy, 0);
        for (j = 0; j < x3pfile->xres; j++) {
            gdouble v = x3pfile->values[id*n + j];

            if (gwy_isnan(v) || gwy_isinf(v) || !x3pfile->valid[id*n + j])
                continue;

            g_array_append_val(validy, v);
            v = j*x3pfile->dx;
            g_array_append_val(validx, v);
        }

        if (!validx->len)
            continue;

        gcmodel = gwy_graph_curve_model_new();
        title = g_strdup_printf("Profile %u", id+1);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description", title,
                     "color", gwy_graph_get_preset_color(id),
                     NULL);
        g_free(title);
        gwy_graph_curve_model_set_data(gcmodel,
                                       (gdouble*)validx->data,
                                       (gdouble*)validy->data,
                                       validx->len);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    g_array_free(validy, TRUE);
    g_array_free(validx, TRUE);

    quark = gwy_app_get_graph_key_for_id(0);
    gwy_container_set_object(container, quark, gmodel);
    g_object_unref(gmodel);
}

static const gchar*
remove_namespace(const gchar *element_name)
{
    const gchar *p;

    if ((p = strchr(element_name, ':')))
        return p+1;
    return element_name;
}

static void
x3p_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                  const gchar *element_name,
                  G_GNUC_UNUSED const gchar **attribute_names,
                  G_GNUC_UNUSED const gchar **attribute_values,
                  gpointer user_data,
                  GError **error)
{
    X3PFile *x3pfile = (X3PFile*)user_data;
    gchar *path;

    element_name = remove_namespace(element_name);
    g_string_append_c(x3pfile->path, '/');
    g_string_append(x3pfile->path, element_name);
    path = x3pfile->path->str;
    gwy_debug("%s", path);

    if (gwy_strequal(path, "/ISO5436_2/Record3/DataLink")
        || gwy_strequal(path, "/ISO5436_2/Record3/DataList")) {
        if (!data_start(x3pfile, error))
            return;
    }

    if (gwy_strequal(path, "/ISO5436_2/Record3/DataList/Datum"))
        x3pfile->seen_datum = FALSE;
}

static void
x3p_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                const gchar *element_name,
                gpointer user_data,
                GError **error)
{
    X3PFile *x3pfile = (X3PFile*)user_data;
    guint n, len = x3pfile->path->len;
    gchar *path = x3pfile->path->str;

    element_name = remove_namespace(element_name);
    n = strlen(element_name);
    g_return_if_fail(g_str_has_suffix(path, element_name));
    g_return_if_fail(len > n);
    g_return_if_fail(path[len-1 - n] == '/');
    gwy_debug("%s", path);

    /* Invalid data points are represented by empty <Datum>. But then
     * x3p_text() is not called at all and we must handle that here. */
    if (gwy_strequal(path, "/ISO5436_2/Record3/DataList/Datum")
        && !x3pfile->seen_datum) {
        if (x3pfile->datapos >= x3pfile->ndata) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Too many DataList items for given "
                          "matrix dimensions."));
            return;
        }
        x3pfile->values[x3pfile->datapos] = 0.0;
        x3pfile->valid[x3pfile->datapos] = FALSE;
        x3pfile->datapos++;
        gwy_debug("invalid Datum");
    }

    g_string_set_size(x3pfile->path, len-1 - n);
}

static void
x3p_text(G_GNUC_UNUSED GMarkupParseContext *context,
         const gchar *text,
         G_GNUC_UNUSED gsize text_len,
         gpointer user_data,
         G_GNUC_UNUSED GError **error)
{
    X3PFile *x3pfile = (X3PFile*)user_data;
    const gchar *semicolon;
    gchar *path = x3pfile->path->str;
    gchar *value;

    if (!strlen(text))
        return;

    /* Data represented directly in XML. */
    if (gwy_strequal(path, "/ISO5436_2/Record3/DataList/Datum")) {
        if (x3pfile->datapos >= x3pfile->ndata) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Too many DataList items for given "
                          "matrix dimensions."));
            return;
        }
        /* There can be a ;-separated list.  And it can contain dummy values
         * for the unused y-axis for profiles (poor design).  Since we do not
         * care about PCL, we only ever want the last value of the list,
         * whatever it is. */
        if ((semicolon = strlenrchr(text, ';', text_len)))
            text = semicolon + 1;

        x3pfile->values[x3pfile->datapos]
            = x3pfile->dz*g_ascii_strtod(text, NULL) + x3pfile->zoff;
        x3pfile->valid[x3pfile->datapos] = TRUE;
        gwy_debug("valid Datum %g", x3pfile->values[x3pfile->datapos]);
        x3pfile->datapos++;
        x3pfile->seen_datum = TRUE;
        return;
    }

    value = g_strdup(text);
    g_strstrip(value);
    g_hash_table_replace(x3pfile->hash, g_strdup(path), value);
}

static gboolean
x3p_parse_main(unzFile *zipfile,
               X3PFile *x3pfile,
               GError **error)
{
    GMarkupParser parser = {
        &x3p_start_element,
        &x3p_end_element,
        &x3p_text,
        NULL,
        NULL,
    };
    GMarkupParseContext *context = NULL;
    guchar *content = NULL, *s;
    gboolean ok = FALSE;

    gwy_debug("calling unzLocateFile() to find main.xml");
    if (unzLocateFile(zipfile, "main.xml", 1) != UNZ_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("File %s is missing in the zip file."), "main.xml");
        return FALSE;
    }

    if (!(content = x3p_get_file_content(zipfile, NULL, error)))
        return FALSE;

    gwy_strkill(content, "\r");
    s = content;
    /* Not seen in the wild but the XML people tend to use BOM in UTF-8... */
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    if (!x3pfile->path)
        x3pfile->path = g_string_new(NULL);
    if (!x3pfile->hash)
        x3pfile->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              g_free, g_free);

    context = g_markup_parse_context_new(&parser, 0, x3pfile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, error))
        goto fail;
    if (!g_markup_parse_context_end_parse(context, error))
        goto fail;

    ok = TRUE;

fail:
    if (context)
        g_markup_parse_context_free(context);
    g_free(content);

    return ok;
}

/* This is the main verification function that checks we have everything we
 * need and the data are of a supported type. */
static gboolean
data_start(X3PFile *x3pfile, GError **error)
{
    static const GwyEnum features[] = {
        { "SUR", X3P_FEATURE_SUR, },
        { "PRF", X3P_FEATURE_PRF, },
        { "PCL", X3P_FEATURE_PCL, },
    };

    gchar *s;

    if (x3pfile->values) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File main.xml contains multiple data elements."));
        return FALSE;
    }

    /* First check axes to get meaningful error messages if their types are
     * not as expected. */
    if (!require_keys(x3pfile->hash, error,
                      "/ISO5436_2/Record1/FeatureType",
                      AXES_PREFIX "/CX/AxisType",
                      AXES_PREFIX "/CY/AxisType",
                      AXES_PREFIX "/CZ/AxisType",
                      NULL))
        return FALSE;

    s = (gchar*)g_hash_table_lookup(x3pfile->hash,
                                    "/ISO5436_2/Record1/FeatureType");
    if ((x3pfile->feature_type
         = gwy_string_to_enum(s, features, G_N_ELEMENTS(features))) == -1) {
        err_UNSUPPORTED(error, "/ISO5436_2/Record1/FeatureType");
        return FALSE;
    }

    if (x3pfile->feature_type != X3P_FEATURE_SUR
        && x3pfile->feature_type != X3P_FEATURE_PRF) {
        err_UNSUPPORTED(error, "/ISO5436_2/Record1/FeatureType");
        return FALSE;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/AxisType");
    if (!gwy_strequal(s, "I")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    /* TRANSLATORS: type and axis are symbols such as I, CX, ...*/
                    _("Only type %s is supported for axis %s."),
                    "I", "CX");
        return FALSE;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/AxisType");
    if (x3pfile->feature_type != X3P_FEATURE_PRF && !gwy_strequal(s, "I")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only type %s is supported for axis %s."),
                    "I", "CY");
        return FALSE;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CZ/AxisType");
    if (!gwy_strequal(s, "A")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only type %s is supported for axis %s."),
                    "A", "CZ");
        return FALSE;
    }

    /* Then check sizes, offsets and steps when we know the grid is regular. */
    if (!require_keys(x3pfile->hash, error,
                      AXES_PREFIX "/CX/Increment",
                      AXES_PREFIX "/CY/Increment",
                      AXES_PREFIX "/CX/Offset",
                      AXES_PREFIX "/CY/Offset",
                      MAT_DIM_PREFIX "/SizeX",
                      MAT_DIM_PREFIX "/SizeY",
                      MAT_DIM_PREFIX "/SizeZ",
                      NULL))
        return FALSE;

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, MAT_DIM_PREFIX "/SizeX");
    x3pfile->xres = atoi(s);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, MAT_DIM_PREFIX "/SizeY");
    x3pfile->yres = atoi(s);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, MAT_DIM_PREFIX "/SizeZ");
    x3pfile->zres = atoi(s);

    gwy_debug("xres=%u, yres=%u, zres=%u",
              x3pfile->xres, x3pfile->yres, x3pfile->zres);

    if (err_DIMENSION(error, x3pfile->xres)
        || err_DIMENSION(error, x3pfile->yres)
        || err_DIMENSION(error, x3pfile->zres))
        return FALSE;

    /* PRF feature types are sets of profiles N×1×M. */
    if (x3pfile->feature_type == X3P_FEATURE_PRF && x3pfile->yres != 1) {
        err_UNSUPPORTED(error, MAT_DIM_PREFIX "/SizeY");
        return FALSE;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/Increment");
    x3pfile->dx = g_ascii_strtod(s, NULL);
    if (!((x3pfile->dx = fabs(x3pfile->dx)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        x3pfile->dx = 1.0;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/Increment");
    x3pfile->dy = g_ascii_strtod(s, NULL);
    if (!((x3pfile->dy = fabs(x3pfile->dy)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        x3pfile->dy = 1.0;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/Offset");
    x3pfile->xoff = g_ascii_strtod(s, NULL);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/Offset");
    x3pfile->yoff = g_ascii_strtod(s, NULL);

    /* Defaults that are good for floating point data conversion.  If a file
     * with floating point data specifies Increment and Offset, we apply them
     * without hesitation.  The behaviour is probably undefined.  */
    x3pfile->dz = 1.0;
    x3pfile->zoff = 0.0;

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CZ/Increment");
    if (s)
        x3pfile->dz = g_ascii_strtod(s, NULL);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CZ/Offset");
    if (s)
        x3pfile->zoff = g_ascii_strtod(s, NULL);

    gwy_debug("dz=%g, zoff=%g", x3pfile->dz, x3pfile->zoff);

    x3pfile->ndata = x3pfile->xres*x3pfile->yres*x3pfile->zres;
    x3pfile->values = g_new(gdouble, x3pfile->ndata);
    x3pfile->valid = g_new(gboolean, x3pfile->ndata);
    x3pfile->datapos = 0;

    return TRUE;
}

static gboolean
read_binary_data(X3PFile *x3pfile, unzFile *zipfile, GError **error)
{
    GwyRawDataType rawtype;
    gsize size;
    guchar *bindata;
    gchar *s;
    guint i;

    s = g_hash_table_lookup(x3pfile->hash, DATA_LINK_PREFIX "/PointDataLink");
    if (!s) {
        err_NO_DATA(error);
        return FALSE;
    }
    gwy_debug("binary data file %s", s);

    if (unzLocateFile(zipfile, s, 1) != UNZ_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("File %s is missing in the zip file."), s);
        return FALSE;
    }

    s = g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CZ/DataType");
    if (!s) {
        err_MISSING_FIELD(error, AXES_PREFIX "CZ/DataType");
        return FALSE;
    }

    if (!x3p_file_get_data_type(s, &rawtype, error))
        return FALSE;

    if (!(bindata = x3p_get_file_content(zipfile, &size, error)))
        return FALSE;

    if (err_SIZE_MISMATCH(error, x3pfile->ndata * gwy_raw_data_size(rawtype),
                          size, TRUE)) {
        g_free(bindata);
        return FALSE;
    }

    gwy_convert_raw_data(bindata, x3pfile->ndata, 1,
                         rawtype, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         x3pfile->values, x3pfile->dz, x3pfile->zoff);
    g_free(bindata);

    for (i = 0; i < x3pfile->ndata; i++)
        x3pfile->valid[i] = TRUE;

    s = g_hash_table_lookup(x3pfile->hash, DATA_LINK_PREFIX "/ValidPointsLink");
    if (!s)
        return TRUE;

    if (unzLocateFile(zipfile, s, 1) != UNZ_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("File %s is missing in the zip file."), s);
        return FALSE;
    }

    if (!(bindata = x3p_get_file_content(zipfile, &size, error)))
        return FALSE;

    if (err_SIZE_MISMATCH(error, (x3pfile->ndata + 7)/8, size, TRUE)) {
        g_free(bindata);
        return FALSE;
    }

    for (i = 0; i < x3pfile->ndata; i++)
        x3pfile->valid[i] = bindata[i/8] & (1 << (i % 8));

    g_free(bindata);

    return TRUE;
}

static GwyContainer*
get_meta(const X3PFile *x3pfile)
{
    GwyContainer *meta = gwy_container_new();

    g_hash_table_foreach(x3pfile->hash, add_meta_record, meta);
    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

static void
add_meta_record(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    const gchar *key = (const gchar*)hkey;
    const gchar *value = (const gchar*)hvalue;
    GwyContainer *meta = (GwyContainer*)user_data;

    if (!gwy_stramong(key,
                      "/ISO5436_2/Record1/Revision",
                      "/ISO5436_2/Record1/FeatureType",
                      "/ISO5436_2/Record2/Date",
                      "/ISO5436_2/Record2/Creator",
                      "/ISO5436_2/Record2/Instrument/Manufacturer",
                      "/ISO5436_2/Record2/Instrument/Model",
                      "/ISO5436_2/Record2/Instrument/Serial",
                      "/ISO5436_2/Record2/Instrument/Version",
                      "/ISO5436_2/Record2/CalibrationDate",
                      "/ISO5436_2/Record2/ProbingSystem/Type",
                      "/ISO5436_2/Record2/ProbingSystem/Identification",
                      "/ISO5436_2/Record2/Comment",
                      NULL)
        && !g_str_has_prefix(key,
                             "/ISO5436_2/Record2/ProbingSystem/Identification/")
        && !g_str_has_prefix(key,
                             "/ISO5436_2/Record1/Axes/Rotation"))
        return;

    key = strrchr(key, '/');
    g_return_if_fail(key);
    key++;
    gwy_container_set_string_by_name(meta, key, (const guchar*)g_strdup(value));
}

static guchar*
x3p_get_file_content(unzFile *zipfile, gsize *contentsize, GError **error)
{
    unz_file_info fileinfo;
    guchar *buffer;
    gulong size;
    glong readbytes;
    gint status;

    gwy_debug("calling unzGetCurrentFileInfo() to figure out buffer size");
    status = unzGetCurrentFileInfo(zipfile, &fileinfo,
                                   NULL, 0,
                                   NULL, 0,
                                   NULL, 0);
    if (status != UNZ_OK) {
        err_MINIZIP(status, error);
        return NULL;
    }

    gwy_debug("calling unzGetCurrentFileInfo()");
    status = unzOpenCurrentFile(zipfile);
    if (status != UNZ_OK) {
        err_MINIZIP(status, error);
        return NULL;
    }

    size = fileinfo.uncompressed_size;
    buffer = g_new(guchar, size + 1);
    gwy_debug("calling unzReadCurrentFile()");
    readbytes = unzReadCurrentFile(zipfile, buffer, size);
    if (readbytes != size) {
        err_MINIZIP(status, error);
        unzCloseCurrentFile(zipfile);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("calling unzCloseCurrentFile()");
    unzCloseCurrentFile(zipfile);

    buffer[size] = '\0';
    if (contentsize)
        *contentsize = size;
    return buffer;
}

static void
x3p_file_free(X3PFile *x3pfile)
{
    if (x3pfile->hash) {
        g_hash_table_destroy(x3pfile->hash);
        x3pfile->hash = NULL;
    }
    if (x3pfile->path) {
        g_string_free(x3pfile->path, TRUE);
        x3pfile->path = NULL;
    }
    g_free(x3pfile->values);
    x3pfile->values = NULL;
    g_free(x3pfile->valid);
    x3pfile->valid = NULL;
}

static gboolean
x3p_file_get_data_type(const gchar *type,
                       GwyRawDataType *rawtype,
                       GError **error)
{
    if (gwy_strequal(type, "I")) {
        *rawtype = GWY_RAW_DATA_SINT16;
        return TRUE;
    }
    if (gwy_strequal(type, "L")) {
        *rawtype = GWY_RAW_DATA_SINT32;
        return TRUE;
    }
    if (gwy_strequal(type, "F")) {
        *rawtype = GWY_RAW_DATA_FLOAT;
        return TRUE;
    }
    if (gwy_strequal(type, "D")) {
        *rawtype = GWY_RAW_DATA_DOUBLE;
        return TRUE;
    }

    err_UNSUPPORTED(error, AXES_PREFIX "/CZ/DataType");
    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
