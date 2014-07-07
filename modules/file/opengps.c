/*
 *  $Id$
 *  Copyright (C) 2012 David Necas (Yeti).
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
#define DEBUG 1
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
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

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

typedef struct {
    GHashTable *hash;
    GString *path;
    gboolean seen_datum;
    guint xres;
    guint yres;
    guint zres;
    guint ndata;
    guint datapos;
    gdouble dx;
    gdouble dy;
    gdouble xoff;
    gdouble yoff;
    gdouble *values;
    gboolean *valid;
} X3PFile;

static gboolean      module_register     (void);
static gint          x3p_detect          (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* x3p_load            (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static gboolean      x3p_parse_main      (unzFile *zipfile,
                                          X3PFile *x3pfile,
                                          GError **error);
static gboolean      data_start          (X3PFile *x3pfile,
                                          GError **error);
static guchar*       x3p_get_file_content(unzFile *zipfile,
                                          gsize *contentsize,
                                          GError **error);
static gboolean      x3p_set_error       (gint status,
                                          GError **error);
static void          x3p_file_free       (X3PFile *x3pfile);

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
    GwyContainer *container = NULL, *meta = NULL;
    X3PFile x3pfile;
    unzFile zipfile;
    guint channelno = 0;

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

    container = gwy_container_new();
    /*
    if (g_hash_table_size(x3pfile.hash)) {
        meta = gwy_container_new();
        g_hash_table_foreach(x3pfile.hash, &add_meta, meta);
    }
    */

fail:
    gwy_debug("calling unzClose()");
    unzClose(zipfile);
    x3p_file_free(&x3pfile);
    gwy_object_unref(meta);
    if (!channelno) {
        gwy_object_unref(container);
        err_NO_DATA(error);
    }

    return container;
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
    gchar *path = x3pfile->path->str;
    gchar *value;

    /* Data represented directly in XML. */
    if (gwy_strequal(path, "/ISO5436_2/Record3/DataList/Datum")) {
        if (x3pfile->datapos >= x3pfile->ndata) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Too many DataList items for given "
                          "matrix dimensions."));
            return;
        }
        x3pfile->values[x3pfile->datapos] = g_ascii_strtod(text, NULL);
        x3pfile->valid[x3pfile->datapos] = TRUE;
        gwy_debug("valid Datum %g", x3pfile->values[x3pfile->datapos]);
        x3pfile->datapos++;
        x3pfile->seen_datum = TRUE;
        return;
    }

    if (!strlen(text))
        return;

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
                    _("File main.xml is missing in the zip file."));
        return FALSE;
    }

    content = x3p_get_file_content(zipfile, NULL, error);
    if (!content)
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

    /* TODO */
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
    gchar *s;

    if (x3pfile->values) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File main.xml contains multiple data elements."));
        return FALSE;
    }

    /* First check axes to get meaningful error messages if their types are
     * not as expected. */
    if (!require_keys(x3pfile->hash, error,
                      AXES_PREFIX "/CX/AxisType",
                      AXES_PREFIX "/CY/AxisType",
                      AXES_PREFIX "/CZ/AxisType",
                      NULL))
        return FALSE;

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/AxisType");
    if (!gwy_strequal(s, "I")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    /* TRANSLATORS: type and axis are symbols such as I, CX, ...*/
                    _("Only type %s is supported for axis %s."),
                    "I", "CX");
        return FALSE;
    }

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/AxisType");
    if (!gwy_strequal(s, "I")) {
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

    gwy_debug("xres=%u, yres=%u, zres=%u\n",
              x3pfile->xres, x3pfile->yres, x3pfile->zres);

    if (err_DIMENSION(error, x3pfile->xres)
        || err_DIMENSION(error, x3pfile->yres)
        || err_DIMENSION(error, x3pfile->zres))
        return FALSE;

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/Increment");
    x3pfile->dx = g_ascii_strtod(s, NULL);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/Increment");
    x3pfile->dy = g_ascii_strtod(s, NULL);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CX/Offset");
    x3pfile->xoff = g_ascii_strtod(s, NULL);

    s = (gchar*)g_hash_table_lookup(x3pfile->hash, AXES_PREFIX "/CY/Offset");
    x3pfile->yoff = g_ascii_strtod(s, NULL);

    x3pfile->ndata = x3pfile->xres*x3pfile->yres*x3pfile->zres;
    x3pfile->values = g_new(gdouble, x3pfile->ndata);
    x3pfile->valid = g_new(gboolean, x3pfile->ndata);
    x3pfile->datapos = 0;

    return TRUE;
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
        x3p_set_error(status, error);
        return NULL;
    }

    gwy_debug("calling unzGetCurrentFileInfo()");
    status = unzOpenCurrentFile(zipfile);
    if (status != UNZ_OK) {
        x3p_set_error(status, error);
        return NULL;
    }

    size = fileinfo.uncompressed_size;
    buffer = g_new(guchar, size + 1);
    gwy_debug("calling unzReadCurrentFile()");
    readbytes = unzReadCurrentFile(zipfile, buffer, size);
    if (readbytes != size) {
        x3p_set_error(status, error);
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

static gboolean
x3p_set_error(gint status, GError **error)
{
    const gchar *errstr = _("Unknown error");

    if (status == UNZ_ERRNO)
        errstr = g_strerror(errno);
    else if (status == UNZ_EOF)
        errstr = _("End of file");
    else if (status == UNZ_END_OF_LIST_OF_FILE)
        errstr = _("End of list of files");
    else if (status == UNZ_PARAMERROR)
        errstr = _("Parameter error");
    else if (status == UNZ_BADZIPFILE)
        errstr = _("Bad zip file");
    else if (status == UNZ_INTERNALERROR)
        errstr = _("Internal error");
    else if (status == UNZ_CRCERROR)
        errstr = _("CRC error");

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Minizip error while reading the zip file: %s."),
                errstr);
    return FALSE;
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
