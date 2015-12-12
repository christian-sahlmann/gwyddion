/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
#ifndef __GWY_FILE_MINIZIP_H__
#define __GWY_FILE_MINIZIP_H__

#include <glib.h>

/****************************************************************************
 * Minizip wrapper
 ****************************************************************************/
#ifdef HAVE_MINIZIP
#include <unzip.h>

#ifdef _MSC_VER
    #include <windows.h>
    #ifdef PATH_MAX
    #undef PATH_MAX
    #endif
    #define PATH_MAX MAX_PATH
#endif

struct _GwyZipFile {
    unzFile *unzfile;
    guint index;
};

typedef struct _GwyZipFile *GwyZipFile;

#ifdef G_OS_WIN32
G_GNUC_UNUSED
static voidpf
gwyzip_open_file_func(G_GNUC_UNUSED voidpf opaque,
                      const char* filename,
                      G_GNUC_UNUSED int mode)
{
    /* Don't implement other modes.  We never write ZIP files with minizip. */
    return (voidpf)gwy_fopen(filename, "rb");
}

G_GNUC_UNUSED
static uLong
gwyzip_read_file_func(G_GNUC_UNUSED voidpf opaque,
                      voidpf stream,
                      void* buf,
                      uLong size)
{
    return fread(buf, 1, size, (FILE*)stream);
}

G_GNUC_UNUSED
static uLong
gwyzip_write_file_func(G_GNUC_UNUSED voidpf opaque,
                       G_GNUC_UNUSED voidpf stream,
                       G_GNUC_UNUSED const void* buf,
                       G_GNUC_UNUSED uLong size)
{
    /* Don't implement writing.  We never write ZIP files with minizip. */
    errno = ENOSYS;
    return 0;
}

G_GNUC_UNUSED
static int
gwyzip_close_file_func(G_GNUC_UNUSED voidpf opaque,
                       voidpf stream)
{
    return fclose((FILE*)stream);
}

G_GNUC_UNUSED
static int
gwyzip_testerror_file_func(G_GNUC_UNUSED voidpf opaque,
                           voidpf stream)
{
    return ferror((FILE*)stream);
}

G_GNUC_UNUSED
static long
gwyzip_tell_file_func(G_GNUC_UNUSED voidpf opaque,
                      voidpf stream)
{
    return ftell((FILE*)stream);
}

G_GNUC_UNUSED
static long
gwyzip_seek_file_func(G_GNUC_UNUSED voidpf opaque,
                      voidpf stream,
                      uLong offset,
                      int origin)
{
    return fseek((FILE*)stream, offset, origin);
}

G_GNUC_UNUSED
static GwyZipFile
gwyzip_open(const gchar *path)
{
    static zlib_filefunc_def ffdef = {
        gwyzip_open_file_func,
        gwyzip_read_file_func,
        gwyzip_write_file_func,
        gwyzip_tell_file_func,
        gwyzip_seek_file_func,
        gwyzip_close_file_func,
        gwyzip_testerror_file_func,
        NULL,
    };
    struct _GwyZipFile *zipfile;
    unzFile *unzfile;

    if (!(unzfile = unzOpen2(path, &ffdef)))
        return NULL;

    zipfile = g_new0(struct _GwyZipFile, 1);
    zipfile->unzfile = unzfile;
    return zipfile;
}
#else
G_GNUC_UNUSED
static GwyZipFile
gwyzip_open(const gchar *path)
{
    struct _GwyZipFile *zipfile;
    unzFile *unzfile;

    if (!(unzfile = unzOpen(path)))
        return NULL;

    zipfile = g_new0(struct _GwyZipFile, 1);
    zipfile->unzfile = unzfile;
    return zipfile;
}
#endif

G_GNUC_UNUSED
static gboolean
err_MINIZIP(gint status, GError **error)
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
                _("Minizip error while reading the zip file: %s (%d)."),
                errstr, status);
    return FALSE;
}

G_GNUC_UNUSED
static void
gwyzip_close(GwyZipFile zipfile)
{
    unzClose(zipfile->unzfile);
    g_free(zipfile);
}

G_GNUC_UNUSED
static gboolean
gwyzip_next_file(GwyZipFile zipfile, GError **error)
{
    gint status;
    if ((status = unzGoToNextFile(zipfile->unzfile)) == UNZ_OK) {
        zipfile->index++;
        return TRUE;
    }
    err_MINIZIP(status, error);
    return FALSE;
}

G_GNUC_UNUSED
static gboolean
gwyzip_first_file(GwyZipFile zipfile, GError **error)
{
    gint status;
    if ((status = unzGoToFirstFile(zipfile->unzfile)) == UNZ_OK) {
        zipfile->index = 0;
        return TRUE;
    }
    err_MINIZIP(status, error);
    return FALSE;
}

G_GNUC_UNUSED
static gboolean
gwyzip_get_current_filename(GwyZipFile zipfile, gchar **filename,
                            GError **error)
{
    unz_file_info fileinfo;
    gint status;
    gchar *filename_buf;
    filename_buf = g_new(gchar, PATH_MAX + 1);

    status = unzGetCurrentFileInfo(zipfile->unzfile, &fileinfo,
                                   filename_buf, PATH_MAX,
                                   NULL, 0, NULL, 0);
    filename_buf[PATH_MAX] = '\0';
    if (status != UNZ_OK) {
        g_free(filename_buf);
        *filename = NULL;
        err_MINIZIP(status, error);
        return FALSE;
    }
    *filename = filename_buf;
    return TRUE;
}

G_GNUC_UNUSED
static gboolean
gwyzip_locate_file(GwyZipFile zipfile, const gchar *filename, gint casesens,
                   GError **error)
{
    gwy_debug("calling unzLocateFile() to find %s", filename);
    if (unzLocateFile(zipfile->unzfile, filename, casesens) != UNZ_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("File %s is missing in the zip file."), filename);
        return FALSE;
    }
    return TRUE;
}

G_GNUC_UNUSED
static guchar*
gwyzip_get_file_content(GwyZipFile zipfile, gsize *contentsize,
                        GError **error)
{
    unz_file_info fileinfo;
    guchar *buffer;
    gulong size;
    glong readbytes;
    gint status;

    gwy_debug("calling unzGetCurrentFileInfo() to figure out buffer size");
    status = unzGetCurrentFileInfo(zipfile->unzfile, &fileinfo,
                                   NULL, 0,
                                   NULL, 0,
                                   NULL, 0);
    if (status != UNZ_OK) {
        err_MINIZIP(status, error);
        return NULL;
    }

    gwy_debug("calling unzGetCurrentFileInfo()");
    status = unzOpenCurrentFile(zipfile->unzfile);
    if (status != UNZ_OK) {
        err_MINIZIP(status, error);
        return NULL;
    }

    size = fileinfo.uncompressed_size;
    gwy_debug("uncompressed_size: %lu", size);
    buffer = g_new(guchar, size + 1);
    gwy_debug("calling unzReadCurrentFile()");
    readbytes = unzReadCurrentFile(zipfile->unzfile, buffer, size);
    if (readbytes != size) {
        err_MINIZIP(status, error);
        unzCloseCurrentFile(zipfile->unzfile);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("calling unzCloseCurrentFile()");
    unzCloseCurrentFile(zipfile->unzfile);

    buffer[size] = '\0';
    if (contentsize)
        *contentsize = size;
    return buffer;
}

#endif

/****************************************************************************
 * Libzip wrapper
 ****************************************************************************/
#ifdef HAVE_LIBZIP
#include <zip.h>

/* This is not defined in 0.11 yet (1.0 is required) but we can live without
 * it. */
#ifndef ZIP_RDONLY
#define ZIP_RDONLY 0
#endif

struct _GwyZipFile {
    struct zip *archive;
    guint index;
    guint nentries;
};

typedef struct _GwyZipFile *GwyZipFile;

G_GNUC_UNUSED
static GwyZipFile
gwyzip_open(const char *path)
{
    struct _GwyZipFile *zipfile;
    struct zip *archive;

    if (!(archive = zip_open(path, ZIP_RDONLY, NULL)))
        return NULL;

    zipfile = g_new0(struct _GwyZipFile, 1);
    zipfile->archive = archive;
    zipfile->nentries = zip_get_num_entries(archive, 0);
    return zipfile;
}

G_GNUC_UNUSED
static void
gwyzip_close(GwyZipFile zipfile)
{
    zip_close(zipfile->archive);
    g_free(zipfile);
}

G_GNUC_UNUSED
static gboolean
err_ZIP_NOFILE(GwyZipFile zipfile, GError **error)
{
    if (zipfile->index >= zipfile->nentries) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Libzip error while reading the zip file: %s."),
                    _("End of list of files"));
        return TRUE;
    }
    return FALSE;
}

G_GNUC_UNUSED
static void
err_ZIP(GwyZipFile zipfile, GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                _("Libzip error while reading the zip file: %s."),
                zip_strerror(zipfile->archive));
}

G_GNUC_UNUSED
static gboolean
gwyzip_next_file(GwyZipFile zipfile, GError **error)
{
    if (err_ZIP_NOFILE(zipfile, error))
        return FALSE;
    zipfile->index++;
    return !err_ZIP_NOFILE(zipfile, error);
}

G_GNUC_UNUSED
static gboolean
gwyzip_first_file(GwyZipFile zipfile, GError **error)
{
    zipfile->index = 0;
    return !err_ZIP_NOFILE(zipfile, error);
}

G_GNUC_UNUSED
static gboolean
gwyzip_get_current_filename(GwyZipFile zipfile, gchar **filename,
                            GError **error)
{
    const char *filename_buf;

    if (err_ZIP_NOFILE(zipfile, error)) {
        *filename = NULL;
        return FALSE;
    }

    filename_buf = zip_get_name(zipfile->archive, zipfile->index,
                                ZIP_FL_ENC_GUESS);
    if (filename_buf) {
        *filename = g_strdup(filename_buf);
        return TRUE;
    }

    err_ZIP(zipfile, error);
    *filename = NULL;
    return FALSE;
}

G_GNUC_UNUSED
static gboolean
gwyzip_locate_file(GwyZipFile zipfile, const gchar *filename, gint casesens,
                   GError **error)
{
    zip_int64_t i;

    i = zip_name_locate(zipfile->archive, filename,
                        ZIP_FL_ENC_GUESS
                        | (casesens ? 0 : ZIP_FL_NOCASE));
    if (i == -1) {
        err_ZIP(zipfile, error);
        return FALSE;
    }
    zipfile->index = i;
    return TRUE;
}

G_GNUC_UNUSED
static guchar*
gwyzip_get_file_content(GwyZipFile zipfile, gsize *contentsize,
                        GError **error)
{
    struct zip_file *file;
    struct zip_stat zst;
    guchar *buffer;

    if (err_ZIP_NOFILE(zipfile, error))
        return NULL;

    zip_stat_init(&zst);
    if (zip_stat_index(zipfile->archive, zipfile->index, 0, &zst) == -1) {
        err_ZIP(zipfile, error);
        return NULL;
    }

    if (!(zst.valid & ZIP_STAT_SIZE)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot obtain the uncompressed file size."));
        return NULL;
    }

    gwy_debug("uncompressed_size: %lu", (gulong)zst.size);
    file = zip_fopen_index(zipfile->archive, zipfile->index, 0);
    if (!file) {
        err_ZIP(zipfile, error);
        return NULL;
    }

    buffer = g_new(guchar, zst.size + 1);
    if (zip_fread(file, buffer, zst.size) != zst.size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot read file contents."));
        zip_fclose(file);
        g_free(buffer);
        return NULL;
    }
    zip_fclose(file);

    buffer[zst.size] = '\0';
    if (contentsize)
        *contentsize = zst.size;
    return buffer;
}
#endif

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
