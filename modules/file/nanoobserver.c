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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanoobserver-spm">
 *   <comment>NanoObserver SPM data</comment>
 *   <glob pattern="*.nao"/>
 *   <glob pattern="*.NAO"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * NanoObserver
 * .nao
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
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC1 "Scan/PK\x03\x04"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define BLOODY_UTF8_BOM "\xef\xbb\xbf"
#define EXTENSION ".nao"

typedef enum {
    DIR_BAD,
    DIR_FORWARD,
    DIR_BACKWARD,
} NAODirection;

typedef struct {
    gchar *name;
    gchar *units;
} NAOStream;

typedef struct {
    guint xres;
    guint yres;
    gdouble xreal;
    gdouble yreal;
    guint nstreams;
    NAOStream *streams;
    /* Workspace */
    gboolean have_resolution;
    gboolean have_size;
    GString *path;
} NAOFile;

static gboolean      module_register     (void);
static gint          nao_detect          (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* nao_load            (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static GwyDataField* nao_read_field      (unzFile *zipfile,
                                          NAOFile *naofile,
                                          guint id);
static gboolean      nao_parse_streams   (unzFile *zipfile,
                                          NAOFile *naofile,
                                          GError **error);
static guchar*       nao_get_file_content(unzFile *zipfile,
                                          gsize *contentsize,
                                          GError **error);
static gboolean      nao_set_error       (gint status,
                                          GError **error);
static void          nao_file_free       (NAOFile *naofile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads NanoObserver .nao files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoobserver",
                           N_("NanoObserver data (.nao)"),
                           (GwyFileDetectFunc)&nao_detect,
                           (GwyFileLoadFunc)&nao_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nao_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    unzFile zipfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Generic ZIP file. */
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It contains directory Scan so this should be somewehre near the begining
     * of the file. */
    if (!gwy_memmem(fileinfo->head, fileinfo->buffer_len, MAGIC1, MAGIC1_SIZE))
        return 0;

    /* We have to realy look inside. */
    if (!(zipfile = unzOpen(fileinfo->name)))
        return 0;

    if (unzLocateFile(zipfile, "Scan/Streams.xml", 1) != UNZ_OK
        || unzLocateFile(zipfile, "Scan/Measure.xml", 1) != UNZ_OK) {
        unzClose(zipfile);
        return 0;
    }

    unzClose(zipfile);

    return 100;
}

static GwyContainer*
nao_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    NAOFile naofile;
    unzFile zipfile;
    NAODirection dir;
    guint id, channelno = 0;
    gint status;

    zipfile = unzOpen(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    gwy_clear(&naofile, 1);
    if (!nao_parse_streams(zipfile, &naofile, error))
        goto fail;

    container = gwy_container_new();
    status = unzGoToFirstFile(zipfile);
    while (status == UNZ_OK) {
        gchar filename_buf[PATH_MAX+1];
        if (unzGetCurrentFileInfo(zipfile, NULL, filename_buf, PATH_MAX,
                                  NULL, 0, NULL, 0) != UNZ_OK) {
            goto fail;
        }
        if (g_str_has_prefix(filename_buf, "Scan/Data/")) {
            const gchar *dataname = filename_buf + strlen("Scan/Data/");
            dir = DIR_BAD;

            gwy_debug("dataname <%s>", dataname);
            for (id = 0; id < naofile.nstreams; id++) {
                guint len = strlen(naofile.streams[id].name);
                if (strncmp(dataname, naofile.streams[id].name, len) == 0) {
                    if (gwy_strequal(dataname + len, "_Left.dat"))
                        dir = DIR_FORWARD;
                    else if (gwy_strequal(dataname + len, "_Right.dat"))
                        dir = DIR_BACKWARD;
                }
                if (dir != DIR_BAD)
                    break;
            }
            if (dir != DIR_BAD) {
                GwyDataField *field = nao_read_field(zipfile, &naofile, id);
                if (field) {
                    GQuark key = gwy_app_get_data_key_for_id(channelno);
                    gchar *strkey, *title;

                    gwy_container_set_object(container, key, field);
                    g_object_unref(field);

                    strkey = g_strdup_printf("/%u/data/title", channelno);
                    title = g_strdup_printf("%s %s",
                                            naofile.streams[id].name,
                                            dir == DIR_FORWARD ? "Left" : "Right");
                    gwy_container_set_string_by_name(container, strkey, title);
                    g_free(strkey);
                    channelno++;
                }
            }

        }
        status = unzGoToNextFile(zipfile);
    }

fail:
    unzClose(zipfile);
    nao_file_free(&naofile);
    if (!channelno) {
        gwy_object_unref(container);
        err_NO_DATA(error);
    }

    return container;
}

static GwyDataField*
nao_read_field(unzFile *zipfile, NAOFile *naofile, guint id)
{
    gsize size, expected_size;
    guint width, height, nscanlines, i, j;
    guchar *buffer = nao_get_file_content(zipfile, &size, NULL);
    const guchar *p = buffer;
    GwyDataField *field;
    const gchar *unit = NULL;
    gdouble *data;

    if (!buffer)
        return NULL;

    if (size < 3*4 + 4 + 4) {
        g_warning("Too short data file (%lu bytes).", (gulong)size);
        g_free(buffer);
        return NULL;
    }

    width = gwy_get_guint32_le(&p);
    height = gwy_get_guint32_le(&p);
    nscanlines = gwy_get_guint32_le(&p);
    gwy_debug("[%u] %u %u %u", id, width, height, nscanlines);

    expected_size = 3*4 + 4*nscanlines*(width + 1);
    if (size != expected_size) {
        g_warning("Data file size %lu does not match expected %lu.",
                  (gulong)size, (gulong)expected_size);
        g_free(buffer);
        return NULL;
    }

    field = gwy_data_field_new(width, nscanlines,
                               naofile->xreal,
                               naofile->yreal*nscanlines/naofile->yres,
                               TRUE);
    data = gwy_data_field_get_data(field);

    for (i = 0; i < nscanlines; i++) {
        guint lineno = gwy_get_guint32_le(&p);
        gdouble *d;

        lineno = MIN(lineno, nscanlines-1);
        d = data + (nscanlines-1 - lineno)*width;
        for (j = width; j; j--)
            *(d++) = gwy_get_gfloat_le(&p);
    }

    g_free(buffer);

    if (gwy_strequal(naofile->streams[id].units, "Meter"))
        unit = "m";
    else if (gwy_strequal(naofile->streams[id].units, "Volt"))
        unit = "V";
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(field), unit);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(field), "m");

    return field;
}

static void
nao_streams_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                          const gchar *element_name,
                          const gchar **attribute_names,
                          const gchar **attribute_values,
                          gpointer user_data,
                          G_GNUC_UNUSED GError **error)
{
    NAOFile *naofile = (NAOFile*)user_data;
    guint i;
    gchar *path;

    g_string_append_c(naofile->path, '/');
    g_string_append(naofile->path, element_name);
    path = naofile->path->str;

    if (gwy_strequal(path, "/MeasureStream/Resolution")) {
        gboolean seen_width = FALSE, seen_height = FALSE;
        gwy_debug("Resolution");
        for (i = 0; attribute_names[i]; i++) {
            if (gwy_strequal(attribute_names[i], "Width")) {
                naofile->xres = atoi(attribute_values[i]);
                seen_width = TRUE;
            }
            else if (gwy_strequal(attribute_names[i], "Height")) {
                naofile->yres = atoi(attribute_values[i]);
                seen_height = TRUE;
            }
        }
        if (seen_width && seen_height) {
            gwy_debug("Resolution %u %u", naofile->xres, naofile->yres);
            naofile->have_resolution = TRUE;
        }
    }

    if (gwy_strequal(path, "/MeasureStream/Size")) {
        gboolean seen_width = FALSE, seen_height = FALSE;
        gwy_debug("Size");
        for (i = 0; attribute_names[i]; i++) {
            if (gwy_strequal(attribute_names[i], "Width")) {
                naofile->xreal = g_ascii_strtod(attribute_values[i], NULL);
                seen_width = TRUE;
            }
            else if (gwy_strequal(attribute_names[i], "Height")) {
                naofile->yreal = g_ascii_strtod(attribute_values[i], NULL);
                seen_height = TRUE;
            }
        }
        if (seen_width && seen_height) {
            gwy_debug("Size %g %g", naofile->xreal, naofile->yreal);
            naofile->have_size = TRUE;
        }
    }

    if (gwy_strequal(path, "/MeasureStream/Channels/Stream")) {
        const gchar *name = NULL, *unit = NULL;
        gwy_debug("Stream");
        for (i = 0; attribute_names[i]; i++) {
            if (gwy_strequal(attribute_names[i], "ID"))
                name = attribute_values[i];
            else if (gwy_strequal(attribute_names[i], "Unit"))
                unit = attribute_values[i];
        }
        if (name && unit) {
            gwy_debug("Adding stream %s [%s]", name, unit);
            naofile->streams = g_renew(NAOStream, naofile->streams,
                                       naofile->nstreams+1);
            naofile->streams[naofile->nstreams].name = g_strdup(name);
            naofile->streams[naofile->nstreams].units = g_strdup(unit);
            naofile->nstreams++;
        }
    }
}

static void
nao_streams_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                        const gchar *element_name,
                        gpointer user_data,
                        G_GNUC_UNUSED GError **error)
{
    NAOFile *naofile = (NAOFile*)user_data;
    guint n = strlen(element_name), len = naofile->path->len;
    gchar *path = naofile->path->str;

    g_return_if_fail(g_str_has_suffix(path, element_name));
    g_return_if_fail(len > n);
    g_return_if_fail(path[len-1 - n] == '/');
    g_string_set_size(naofile->path, len-1 - n);
}

static gboolean
nao_parse_streams(unzFile *zipfile,
                  NAOFile *naofile,
                  GError **error)
{
    GMarkupParser parser = {
        &nao_streams_start_element,
        &nao_streams_end_element,
        NULL,
        NULL,
        NULL,
    };
    GMarkupParseContext *context = NULL;
    guchar *content = NULL, *s;
    gboolean ok = FALSE;

    if (unzLocateFile(zipfile, "Scan/Streams.xml", 1) != UNZ_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("File Scan/Streams.xml is missing in the zip file."));
        return FALSE;
    }

    content = nao_get_file_content(zipfile, NULL, error);
    if (!content)
        return FALSE;

    gwy_strkill(content, "\r");
    s = content;
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    if (!naofile->path)
        naofile->path = g_string_new(NULL);

    context = g_markup_parse_context_new(&parser, 0, naofile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, error))
        goto fail;
    if (!g_markup_parse_context_end_parse(context, error))
        goto fail;

    if (!naofile->have_resolution) {
        err_MISSING_FIELD(error, "Resolution");
        goto fail;
    }
    if (!naofile->have_size) {
        err_MISSING_FIELD(error, "Size");
        goto fail;
    }
    if (!naofile->nstreams) {
        err_NO_DATA(error);
        goto fail;
    }
    ok = TRUE;

fail:
    if (context)
        g_markup_parse_context_free(context);
    g_free(content);

    return ok;
}

static guchar*
nao_get_file_content(unzFile *zipfile, gsize *contentsize, GError **error)
{
    unz_file_info fileinfo;
    guchar *buffer;
    gulong size;
    glong readbytes;
    gint status;

    status = unzGetCurrentFileInfo(zipfile, &fileinfo,
                                   NULL, 0,
                                   NULL, 0,
                                   NULL, 0);
    if (status != UNZ_OK) {
        nao_set_error(status, error);
        return NULL;
    }

    status = unzOpenCurrentFile(zipfile);
    if (status != UNZ_OK) {
        nao_set_error(status, error);
        return NULL;
    }

    size = fileinfo.uncompressed_size;
    buffer = g_new(guchar, size + 1);
    readbytes = unzReadCurrentFile(zipfile, buffer, size);
    if (readbytes != size) {
        nao_set_error(status, error);
        unzCloseCurrentFile(zipfile);
        g_free(buffer);
        return NULL;
    }
    unzCloseCurrentFile(zipfile);

    buffer[size] = '\0';
    if (contentsize)
        *contentsize = size;
    return buffer;
}

static gboolean
nao_set_error(gint status, GError **error)
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
nao_file_free(NAOFile *naofile)
{
    guint i;

    for (i = 0; i < naofile->nstreams; i++) {
        g_free(naofile->streams[i].name);
        g_free(naofile->streams[i].units);
    }
    if (naofile->path) {
        g_string_free(naofile->path, TRUE);
        naofile->path = NULL;
    }
    g_free(naofile->streams);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
