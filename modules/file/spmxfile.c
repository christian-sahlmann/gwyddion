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
#define DEBUG 1
/**
 * [FILE-MAGIC-USERGUIDE]
 * ATC SPMxFormat data
 * .spm
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
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

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC1 "main.xml"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 ".chnl.raw"
#define MAGIC2_SIZE (sizeof(MAGIC2)-1)
#define BLOODY_UTF8_BOM "\xef\xbb\xbf"
#define EXTENSION ".spm"

typedef struct {
    gchar *id;
    gchar *name;
    gchar *filename;
    guint blocksize;
    guint dim;
    GPtrArray *subimages;
    GwySIUnit *unitxy[2];
    guint xyres[2];
    gdouble xyscale[2];
    GwySIUnit *unitz;
    gdouble zoff, zscale;
} SPMXStream;

typedef struct {
    GHashTable *hash;
    GString *path;
    GString *varid;
    GString *str;
    GArray *streams;
} SPMXFile;

static gboolean      module_register      (void);
static gint          spmx_detect          (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* spmx_load            (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      spmx_parse_main      (unzFile *zipfile,
                                           SPMXFile *spmxfile,
                                           GError **error);
static gboolean      read_binary_data     (const SPMXFile *spmxfile,
                                           unzFile *zipfile,
                                           GwyContainer *container,
                                           GError **error);
static guchar*       spmx_get_file_content(unzFile *zipfile,
                                           gsize *contentsize,
                                           GError **error);
static void          spmx_file_free       (SPMXFile *spmxfile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads ATC SPMxFormat files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("spmxfile",
                           N_("ATC SPMxFormat data (.spm)"),
                           (GwyFileDetectFunc)&spmx_detect,
                           (GwyFileLoadFunc)&spmx_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
spmx_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    unzFile zipfile;
    guchar *content;
    gint score = 0;

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

    /* We have to realy look inside.  And since main.xml is a popular name
     * for the main XML document within such files, we also have to see if
     * we find "SPMxFormat" somewehre near the begining of the file. */
    if ((zipfile = unzOpen(fileinfo->name))) {
        if (unzLocateFile(zipfile, "main.xml", 1) == UNZ_OK) {
            if ((content = spmx_get_file_content(zipfile, NULL, NULL))) {
                if (g_strstr_len(content, 4096, "SPMxFormat"))
                    score = 100;
                g_free(content);
            }
        }
        unzClose(zipfile);
    }

    return score;
}

static GwyContainer*
spmx_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    SPMXFile spmxfile;
    unzFile zipfile;

    zipfile = unzOpen(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    gwy_clear(&spmxfile, 1);
    if (!spmx_parse_main(zipfile, &spmxfile, error))
        goto fail;

    if (!spmxfile.streams->len) {
        err_NO_DATA(error);
        goto fail;
    }

    container = gwy_container_new();
    if (!read_binary_data(&spmxfile, zipfile, container, error))
        gwy_object_unref(container);

fail:
    unzClose(zipfile);
    spmx_file_free(&spmxfile);

    return container;
}

static gboolean
read_binary_data(const SPMXFile *spmxfile,
                 unzFile *zipfile,
                 GwyContainer *container,
                 GError **error)
{
    GArray *streams = spmxfile->streams;
    GwyDataField *dfield;
    guchar *content;
    gchar *key, *title, *subimage;
    gsize contentsize, expected_size;
    GwySIUnit *unit;
    guint i, j, n;
    gint id = 0;

    for (i = 0; i < streams->len; i++) {
        SPMXStream *stream = &g_array_index(streams, SPMXStream, i);

        if (unzLocateFile(zipfile, stream->filename, 1) != UNZ_OK) {
            g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                        _("File %s is missing in the zip file."),
                        stream->filename);
            return FALSE;
        }

        if (!(content = spmx_get_file_content(zipfile, &contentsize, error)))
            return FALSE;

        n = stream->xyres[0]*stream->xyres[1];
        expected_size = n*stream->blocksize*stream->subimages->len;
        if (err_SIZE_MISMATCH(error, expected_size, contentsize, TRUE)) {
            g_free(content);
            return FALSE;
        }

        for (j = 0; j < stream->subimages->len; j++) {
            dfield = gwy_data_field_new(stream->xyres[0], stream->xyres[1],
                                        stream->xyscale[0]*stream->xyres[0],
                                        stream->xyscale[1]*stream->xyres[1],
                                        FALSE);
            gwy_convert_raw_data(content + j*n*stream->blocksize, n, 1,
                                 GWY_RAW_DATA_SINT16,
                                 GWY_BYTE_ORDER_LITTLE_ENDIAN,
                                 gwy_data_field_get_data(dfield),
                                 stream->zscale, stream->zoff);

            unit = gwy_data_field_get_si_unit_xy(dfield);
            gwy_serializable_clone(G_OBJECT(stream->unitxy[0]), G_OBJECT(unit));

            unit = gwy_data_field_get_si_unit_z(dfield);
            gwy_serializable_clone(G_OBJECT(stream->unitz), G_OBJECT(unit));

            gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                     dfield);
            g_object_unref(dfield);

            key = g_strdup_printf("/%d/data/title", id);
            subimage = (gchar*)g_ptr_array_index(stream->subimages, j);
            title = g_strdup_printf("%s %s", stream->name, subimage);
            gwy_container_set_string_by_name(container, key, title);
            g_free(key);

            id++;
        }

        g_free(content);
    }

    return TRUE;
}

static SPMXStream*
current_stream(SPMXFile *spmxfile)
{
    GArray *streams = spmxfile->streams;
    guint nstreams = streams->len;

    if (!nstreams)
        return NULL;

    return &g_array_index(streams, SPMXStream, nstreams-1);
}

static const gchar*
find_attribute(const gchar **attribute_names, const gchar **attribute_values,
               const gchar *attrname)
{
    guint i;

    if (!attribute_names)
        return NULL;

    for (i = 0; attribute_names[i]; i++) {
        if (gwy_strequal(attribute_names[i], attrname))
            return attribute_values[i];
    }

    return NULL;
}

static gboolean
require_attributes(const gchar *path,
                   const gchar **attribute_names,
                   const gchar **attribute_values,
                   GError **error,
                   ...)
{
    va_list ap;
    const gchar *attrname;
    gchar *errpath;

    va_start(ap, error);
    while ((attrname = va_arg(ap, const gchar *))) {
        if (!find_attribute(attribute_names, attribute_values, attrname)) {
            errpath = g_strconcat(path, "::", attrname, NULL);
            err_MISSING_FIELD(error, errpath);
            g_free(errpath);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

static void
spmx_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                   const gchar *element_name,
                   const gchar **attribute_names,
                   const gchar **attribute_values,
                   gpointer user_data,
                   GError **error)
{
    SPMXFile *spmxfile = (SPMXFile*)user_data;
    SPMXStream *stream = current_stream(spmxfile);
    const gchar *value;
    gchar *path;
    gint power10;

    g_string_append_c(spmxfile->path, '/');
    g_string_append(spmxfile->path, element_name);
    path = spmxfile->path->str;
    gwy_debug("%s", path);

    if (gwy_strequal(path, "/spmx/stream")) {
        SPMXStream newstream;

        if (!require_attributes(path, attribute_names, attribute_values, error,
                                "id", "name", "blocksize", NULL))
            return;

        gwy_clear(&newstream, 1);
        value = find_attribute(attribute_names, attribute_values, "id");
        newstream.id = g_strdup(value);
        value = find_attribute(attribute_names, attribute_values, "name");
        newstream.name = g_strdup(value);
        value = find_attribute(attribute_names, attribute_values, "blocksize");
        newstream.blocksize = atoi(value);

        newstream.subimages = g_ptr_array_new();
        g_array_append_val(spmxfile->streams, newstream);
    }
    else if (gwy_strequal(path, "/spmx/var")) {
        if ((value = find_attribute(attribute_names, attribute_values, "id")))
            g_string_assign(spmxfile->varid, value);
        else
            g_string_truncate(spmxfile->varid, 0);
    }
    else if (gwy_strequal(path, "/spmx/stream/axis")) {
        g_return_if_fail(stream);
        /* There can be multiple records (e.g. Height also as voltage).  Take
         * the first.  */
        if (stream->unitz)
            return;

        if (!require_attributes(path, attribute_names, attribute_values, error,
                                "scale", "units", NULL))
            return;

        value = find_attribute(attribute_names, attribute_values, "scale");
        stream->zscale = g_ascii_strtod(value, NULL);
        value = find_attribute(attribute_names, attribute_values, "units");
        stream->unitz = gwy_si_unit_new_parse(value, &power10);
        stream->zscale *= pow10(power10);

        if ((value = find_attribute(attribute_names, attribute_values,
                                    "start")))
            stream->zoff = g_ascii_strtod(value, NULL) * pow10(power10);
    }
    else if (gwy_strequal(path, "/spmx/stream/dimension")) {
        g_return_if_fail(stream);
        if (!require_attributes(path, attribute_names, attribute_values, error,
                                "length", NULL))
            return;

        /* XXX: More than two axes? */
        if (stream->dim >= G_N_ELEMENTS(stream->xyres)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Only two-dimensional images are supported."));
            return;
        }

        value = find_attribute(attribute_names, attribute_values, "length");
        stream->xyres[stream->dim] = atoi(value);
        gwy_debug("res[%u] %u", stream->dim, stream->xyres[stream->dim]);
    }
    else if (gwy_strequal(path, "/spmx/stream/dimension/axis")) {
        g_return_if_fail(stream);
        /* There can be multiple records (e.g. distance also as time).  Take
         * the first.  */
        if (stream->dim >= G_N_ELEMENTS(stream->xyres)
            || stream->unitxy[stream->dim])
            return;

        if (!require_attributes(path, attribute_names, attribute_values, error,
                                "scale", "units", NULL))
            return;

        value = find_attribute(attribute_names, attribute_values, "scale");
        stream->xyscale[stream->dim] = g_ascii_strtod(value, NULL);
        value = find_attribute(attribute_names, attribute_values, "units");
        stream->unitxy[stream->dim] = gwy_si_unit_new_parse(value, &power10);
        stream->xyscale[stream->dim]*= pow10(power10);
    }
    else if (gwy_strequal(path, "/spmx/stream/data")) {
        g_return_if_fail(stream);
        if (!require_attributes(path, attribute_names, attribute_values, error,
                                "content", "ref", NULL))
            return;

        value = find_attribute(attribute_names, attribute_values, "content");
        if (!gwy_strequal(value, "void")) {
            err_UNSUPPORTED(error, "/spmx/stream/data::content");
            return;
        }
        value = find_attribute(attribute_names, attribute_values, "ref");
        /* XXX: The file names are in fact bound to the ids via ENTITY.  We
         * just assume the naming is consistent. */
        stream->filename = g_strconcat(value, ".raw", NULL);
    }
}

static void
spmx_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                 const gchar *element_name,
                 gpointer user_data,
                 G_GNUC_UNUSED GError **error)
{
    SPMXFile *spmxfile = (SPMXFile*)user_data;
    guint n, len = spmxfile->path->len;
    gchar *path = spmxfile->path->str;

    n = strlen(element_name);
    g_return_if_fail(g_str_has_suffix(path, element_name));
    g_return_if_fail(len > n);
    g_return_if_fail(path[len-1 - n] == '/');
    gwy_debug("%s", path);

    if (gwy_strequal(path, "/spmx/stream/dimension")) {
        SPMXStream *stream = current_stream(spmxfile);
        stream->dim++;
    }

    g_string_set_size(spmxfile->path, len-1 - n);
}

static void
spmx_text(G_GNUC_UNUSED GMarkupParseContext *context,
          const gchar *text,
          G_GNUC_UNUSED gsize text_len,
          gpointer user_data,
          G_GNUC_UNUSED GError **error)
{
    SPMXFile *spmxfile = (SPMXFile*)user_data;
    SPMXStream *stream = current_stream(spmxfile);
    gchar *path = spmxfile->path->str;
    gchar *varid = spmxfile->varid->str;
    GString *str = spmxfile->str;

    if (!strlen(text))
        return;

    /* I thought this would somehow refer to <var>s, but many mentioned in
     * <depends> are blatantly missing...  So just ignore them. */
    if (gwy_strequal(path, "/spmx/stream/depends"))
        return;

    g_string_assign(str, text);
    g_strstrip(str->str);
    if (!strlen(str->str))
        return;

    if (gwy_strequal(path, "/spmx/var") && strlen(varid)) {
        gwy_debug("var %s = <%s>", varid, str->str);
        g_hash_table_insert(spmxfile->hash,
                            g_strdup(varid), g_strdup(str->str));
    }
    else if (gwy_strequal(path, "/spmx/stream/subimage")) {
        gwy_debug("subimage <%s>", str->str);
        g_return_if_fail(stream);
        g_ptr_array_add(stream->subimages, g_strdup(str->str));
    }
    else {
        gwy_debug("%s <%s>", path, str->str);
    }
}

static gboolean
spmx_parse_main(unzFile *zipfile,
                SPMXFile *spmxfile,
                GError **error)
{
    GMarkupParser parser = {
        &spmx_start_element,
        &spmx_end_element,
        &spmx_text,
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

    if (!(content = spmx_get_file_content(zipfile, NULL, error)))
        return FALSE;

    gwy_strkill(content, "\r");
    s = content;
    /* Not seen in the wild but the XML people tend to use BOM in UTF-8... */
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    spmxfile->path = g_string_new(NULL);
    spmxfile->varid = g_string_new(NULL);
    spmxfile->str = g_string_new(NULL);
    spmxfile->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, g_free);
    spmxfile->streams = g_array_new(FALSE, FALSE, sizeof(SPMXStream));

    context = g_markup_parse_context_new(&parser, 0, spmxfile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, error))
        goto fail;
    if (!g_markup_parse_context_end_parse(context, error))
        goto fail;

    /* TODO: We still need to do some sanity checks, for instance whether
     * the streams have set dimensions and units. */
    ok = TRUE;

fail:
    if (context)
        g_markup_parse_context_free(context);
    g_free(content);

    return ok;
}

static guchar*
spmx_get_file_content(unzFile *zipfile, gsize *contentsize, GError **error)
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
spmx_file_free(SPMXFile *spmxfile)
{
    if (spmxfile->hash)
        g_hash_table_destroy(spmxfile->hash);
    if (spmxfile->path)
        g_string_free(spmxfile->path, TRUE);
    if (spmxfile->varid)
        g_string_free(spmxfile->varid, TRUE);
    if (spmxfile->str)
        g_string_free(spmxfile->str, TRUE);
    if (spmxfile->streams) {
        guint i, j;

        for (i = 0; i < spmxfile->streams->len; i++) {
            SPMXStream *stream = &g_array_index(spmxfile->streams,
                                                SPMXStream, i);

            g_free(stream->id);
            g_free(stream->name);
            g_free(stream->filename);
            gwy_object_unref(stream->unitxy[0]);
            gwy_object_unref(stream->unitxy[1]);
            gwy_object_unref(stream->unitz);

            for (j = 0; j < stream->subimages->len; j++)
                g_free(g_ptr_array_index(stream->subimages, j));
            g_ptr_array_free(stream->subimages, TRUE);
        }
        g_array_free(spmxfile->streams, TRUE);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
