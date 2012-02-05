/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Necas (Yeti), Petr Klapetek,
 *  Daniil Bratashov (dn2010)
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-oldmda-spm">
 *   <comment>NTMDT old MDA Spectra data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="&lt;?xml">
 *       <match type="string" offset="20:60" value="&lt;MDAList&gt;"/>
 *     </match>
 *   </magic>
 *   <glob pattern="*.sxml"/>
 *   <glob pattern="*.SXML"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * oldmda
 * .sxml .dat
 * Read
 **/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".sxml"

#define MAGIC1 "<?xml"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 "<MDAList>"
#define MAGIC2_SIZE (sizeof(MAGIC2) - 1)

#if GLIB_CHECK_VERSION(2, 12, 0)
#define TREAT_CDATA_AS_TEXT G_MARKUP_TREAT_CDATA_AS_TEXT
#else
#define TREAT_CDATA_AS_TEXT 0
#endif

typedef enum {
    MDA_XML_NONE             = 0,
    MDA_XML_ARRAYSIZE        = 1,
    MDA_XML_DATACELL_MEMSIZE = 2,
    MDA_XML_NAME             = 3,
    MDA_XML_UNITNAME         = 4,
    MDA_XML_MININDEX         = 5,
    MDA_XML_MAXINDEX         = 6,
    MDA_XML_DATAARRAY        = -1,
    MDA_XML_DATANAME         = -2
} MDAXMLParamType;

typedef struct {
    gchar   *name;
    gchar   *unitname;
    gdouble  bias;
    gdouble  scale;
    gint     minindex;
    gint     maxindex;
} MDAAxis;

typedef struct {
    guint           arraysize;
    guint           datacellmemsize;
    guint           res;
    gdouble         *data;
    gchar           *dataname;
    GArray          *axes;
    gint            numaxes;
    MDAXMLParamType flag;
} MDAXMLParams;

typedef struct {
    guint         xres;
    guint         yres;
    gdouble       xreal;
    gdouble       yreal;
    guint         res;
    gdouble      *xdata;
    GwyContainer *data;
}OldMDAFile;

static gboolean      module_register     (void);
static gint          oldmda_detect       (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* oldmda_load         (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static void          oldmda_free         (MDAXMLParams *par);
static void          oldmda_read_params  (MDAXMLParams *par,
                                          OldMDAFile *mdafile);
static void          oldmda_read_data    (OldMDAFile *mdafile,
                                          const gchar *buffer);
static gchar*      oldmda_find_data_name (const gchar *headername,
                                          const gchar *dataname);
static void          start_element       (GMarkupParseContext *context,
                                          const gchar *element_name,
                                          const gchar **attribute_names,
                                          const gchar **attribute_values,
                                          gpointer user_data,
                                          GError **error);
static void          end_element         (GMarkupParseContext *context,
                                          const gchar *element_name,
                                          gpointer user_data,
                                          GError **error);
static void          parse_text          (GMarkupParseContext *context,
                                          const gchar *text,
                                          gsize text_len,
                                          gpointer user_data,
                                          GError **error);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports old NTMDT MDA Spectra files."),
    "dn2010 <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Daniil Bratashov (dn2010)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("oldmda",
                           N_("NTMDT old MDA Spectra data (.sxml .dat)"),
                           (GwyFileDetectFunc)&oldmda_detect,
                           (GwyFileLoadFunc)&oldmda_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gboolean
check_magic(const gchar *header)
{
    return (memcmp(header, MAGIC1, MAGIC1_SIZE) == 0
            && strstr(header, MAGIC2) != NULL);
}

static gint
oldmda_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    return check_magic(fileinfo->head) ? 100 : 0;
}

static GwyContainer*
oldmda_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    OldMDAFile mdafile;
    gchar *head, *buffer = NULL;
    gchar *buffer2 = NULL;
    gchar *dataname = NULL, *dname;
    GMarkupParser parser = { start_element, end_element, parse_text, NULL, NULL };
    GMarkupParseContext *context;
    gsize size, size2;
    gdouble xdata[1024];
    MDAXMLParams params;
    GError *err = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    gwy_clear(&mdafile, 1);
    head = g_strndup(buffer, 1024);
    if (!check_magic(head)) {
        err_FILE_TYPE(error, "NTMDT old MDA");
        g_free(head);
        goto fail;
    }
    g_free(head);

    params.data = xdata;
    params.numaxes = 0;
    params.axes = g_array_new(FALSE, FALSE, sizeof(MDAAxis));
    params.flag = MDA_XML_NONE;
    context = g_markup_parse_context_new(&parser, TREAT_CDATA_AS_TEXT,
                                         &params, NULL);
    if (!g_markup_parse_context_parse(context, buffer, size, &err)
     || !g_markup_parse_context_end_parse(context, &err)) {
        g_clear_error(&err);
        g_markup_parse_context_free(context);
    }
    else {
        g_markup_parse_context_free(context);
    }

    if (params.axes->len != 4) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Incorrect number of axes in parameter file."));
        goto fail;
    }

    oldmda_read_params(&params, &mdafile);

    dname = g_strdelimit(params.dataname, "\\/",G_DIR_SEPARATOR);
    if (!(dataname = oldmda_find_data_name(filename, dname))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("No corresponding data file was found for header file."));
        goto fail;
    }

    if (!g_file_get_contents(dataname, &buffer2, &size2, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot load data file: %s"), err->message);
        g_clear_error(&err);
        goto fail;
    }

    if ((size2 != params.arraysize * params.datacellmemsize)
     || (params.arraysize != mdafile.xres * mdafile.yres * mdafile.res)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Data file too small."));
        g_clear_error(&err);
        goto fail2;
    }

    container = gwy_container_new();
    mdafile.data = container;
    oldmda_read_data(&mdafile, buffer2);

fail2:
    g_free(buffer2);
fail:
    oldmda_free(&params);
    g_free(buffer);

    return container;
}

static void
oldmda_free(MDAXMLParams *par)
{
    guint i;

    if (par->axes) {
        for (i = 0; i < par->axes->len; i++) {
            MDAAxis *axis = &g_array_index(par->axes, MDAAxis, i);
            g_free(axis->name);
            g_free(axis->unitname);
        }
        g_array_free(par->axes, TRUE);
    }

    g_free(par->dataname);
}

static void
oldmda_read_params(MDAXMLParams *par, OldMDAFile *mdafile)
{
    MDAAxis axis;
    mdafile->res = par->res;
    mdafile->xdata = par->data;
    axis = g_array_index(par->axes, MDAAxis, 1);
    mdafile->xres = axis.maxindex - axis.minindex + 1;
    if (mdafile->xres < 1)
        mdafile->xres = 1;
    if (axis.scale <= 0.0)
        axis.scale = 1.0;
    mdafile->xreal = axis.scale*mdafile->xres;
    axis = g_array_index(par->axes, MDAAxis, 2);
    mdafile->yres = axis.maxindex - axis.minindex + 1;
    if (mdafile->yres < 1)
        mdafile->yres = 1;
    if (axis.scale <= 0.0)
        axis.scale = 1.0;
    mdafile->yreal = axis.scale*mdafile->yres;
}

static void oldmda_read_data(OldMDAFile *mdafile, const gchar *buffer)
{
    GwyDataField *dfield;
    gdouble *zdata;
    gdouble yspectra [1024];
    gint i, j, k;
    const guchar *p;

    p = buffer;
    dfield = gwy_data_field_new(mdafile->xres, mdafile->yres,
                                mdafile->xreal, mdafile->yreal,
                                TRUE);
    zdata = gwy_data_field_get_data(dfield);

    for (i = 0; i < mdafile->yres; i++)
        for (j = 0; j < mdafile->xres; j++) {
            *zdata = 0.0;
            for (k = 0; k < mdafile->res; k++) {
                yspectra[k] = (gdouble)gwy_get_gint32_le(&p);
                if (yspectra[k] > *zdata)
                    *zdata = yspectra[k];
            }
            zdata++;
        }
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    gwy_container_set_object_by_name(mdafile->data, "/0/data", dfield);
    gwy_container_set_string_by_name(mdafile->data, "/0/data/title",
                                         g_strdup("Image"));
    g_object_unref(dfield);
}

static gchar*
oldmda_find_data_name(const gchar *headername, const gchar *dataname)
{
    gchar *dirname = g_path_get_dirname(headername);
    gchar *data_name = g_path_get_basename(dataname);
    gchar *dname, *filename;

    filename = g_build_filename(dirname, data_name, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(dirname);
        return filename;
    }
    g_free(filename);

    dname = g_ascii_strup(data_name, -1);
    filename = g_build_filename(dirname, dname, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(dname);
        g_free(dirname);
        return filename;
    }
    g_free(dname);
    g_free(filename);

    dname = g_ascii_strdown(data_name, -1);
    filename = g_build_filename(dirname, dname, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(dname);
        g_free(dirname);
        return filename;
    }
    g_free(dname);
    g_free(filename);
    g_free(dirname);

    gwy_debug("failed");

    return NULL;
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              G_GNUC_UNUSED const gchar **attribute_names,
              G_GNUC_UNUSED const gchar **attribute_values,
              gpointer user_data,
              G_GNUC_UNUSED GError **error)
{
    const gchar **name_cursor = attribute_names;
    const gchar **value_cursor = attribute_values;
    MDAAxis axis;
    gchar *value;

    MDAXMLParams *params = (MDAXMLParams *)user_data;
    if (params->flag != MDA_XML_NONE) {
        /* error */
    }
    else {
        if (gwy_strequal(element_name, "ArraySize")) {
            params->flag = MDA_XML_ARRAYSIZE;
        }
        else if (gwy_strequal(element_name, "DataCellMemSize")) {
            params->flag = MDA_XML_DATACELL_MEMSIZE;
        }
        else if (gwy_strequal(element_name, "MinIndex")) {
            params->flag = MDA_XML_MININDEX;
        }
        else if (gwy_strequal(element_name, "MaxIndex")) {
            params->flag = MDA_XML_MAXINDEX;
        }
        else if (gwy_strequal(element_name, "Name")) {
            params->flag = MDA_XML_NAME;
        }
        else if (gwy_strequal(element_name, "Data")) {
            params->flag = MDA_XML_DATANAME;
        }
        else if (gwy_strequal(element_name, "Calibration")) {
            params->numaxes++;
            gwy_clear(&axis, 1);

            while (*name_cursor) {
                if (gwy_strequal(*name_cursor, "UnitName")) {
                    axis.unitname = g_strdup(*value_cursor);
                }
                else if (gwy_strequal(*name_cursor, "Bias")) {
                    value = g_strdup(*value_cursor);
                    axis.bias = g_ascii_strtod(
                        g_strdelimit(value, ",.", '.'), NULL);
                    g_free(value);
                }
                else if (gwy_strequal(*name_cursor, "Scale")) {
                    value = g_strdup(*value_cursor);
                    axis.scale = g_ascii_strtod(
                        g_strdelimit(value, ",.", '.'), NULL);
                    g_free(value);
                }
                name_cursor++;
                value_cursor++;
            }
            g_array_append_val(params->axes, axis);
        }
        else if (gwy_strequal(element_name, "Array")) {
            params->flag = MDA_XML_DATAARRAY;
            while (*name_cursor) {
                if (gwy_strequal(*name_cursor, "Count"))
                    params->res = atoi(*value_cursor);

                name_cursor++;
                value_cursor++;
            }
        }
    }
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            G_GNUC_UNUSED const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    MDAXMLParams *params = (MDAXMLParams*)user_data;

    params->flag = MDA_XML_NONE;
}

static void
parse_text(G_GNUC_UNUSED GMarkupParseContext *context,
           const gchar *value,
           G_GNUC_UNUSED gsize value_len,
           gpointer user_data,
           G_GNUC_UNUSED GError **error)
{
    MDAXMLParams *params = (MDAXMLParams*)user_data;
    MDAAxis *axis;
    gchar *line;
    gdouble val;
    gint i;
    axis = &g_array_index(params->axes, MDAAxis, params->numaxes - 1);

    if (params->flag == MDA_XML_NONE) {
        /* error */
    }
    else if (params->flag == MDA_XML_ARRAYSIZE) {
        params->arraysize = atoi(value);
    }
    else if (params->flag == MDA_XML_DATACELL_MEMSIZE) {
        params->datacellmemsize = atoi(value);
    }
    else if (params->flag == MDA_XML_DATANAME) {
        params->dataname = g_strdup(value);
    }
    else if (params->flag == MDA_XML_MININDEX) {
        axis->minindex = atoi(value);
    }
    else if (params->flag == MDA_XML_MAXINDEX) {
        axis->maxindex = atoi(value);
    }
    else if (params->flag == MDA_XML_NAME) {
        axis->name = g_strdup(value);
    }
    else if (params->flag == MDA_XML_DATAARRAY) {
        line = (gchar *)value;
        if (!params->res) {
            /* Error */
        }
        else {
            params->res = params->res > 1024 ? 1024 : params->res;
            for (i = 0; i < params->res; i++) {
                val = g_ascii_strtod(g_strdelimit(line, ",.", '.'), &line);
                line += 2; /* skip ". " between values */
                params->data[i] = val;
            }
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
