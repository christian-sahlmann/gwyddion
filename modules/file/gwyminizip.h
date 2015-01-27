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
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
G_GNUC_UNUSED
static voidpf
gwyminizip_open_file_func(G_GNUC_UNUSED voidpf opaque,
                          const char* filename,
                          G_GNUC_UNUSED int mode)
{
    /* Don't implement other modes.  We never write ZIP files with minizip. */
    return (voidpf)g_fopen(filename, "rb");
}

G_GNUC_UNUSED
static uLong
gwyminizip_read_file_func(G_GNUC_UNUSED voidpf opaque,
                          voidpf stream,
                          void* buf,
                          uLong size)
{
    return fread(buf, 1, size, (FILE*)stream);
}

G_GNUC_UNUSED
static uLong
gwyminizip_write_file_func(G_GNUC_UNUSED voidpf opaque,
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
gwyminizip_close_file_func(G_GNUC_UNUSED voidpf opaque,
                           voidpf stream)
{
    return fclose((FILE*)stream);
}

G_GNUC_UNUSED
static int
gwyminizip_testerror_file_func(G_GNUC_UNUSED voidpf opaque,
                               voidpf stream)
{
    return ferror((FILE*)stream);
}

G_GNUC_UNUSED
static long
gwyminizip_tell_file_func(G_GNUC_UNUSED voidpf opaque,
                          voidpf stream)
{
    return ftell((FILE*)stream);
}

G_GNUC_UNUSED
static long
gwyminizip_seek_file_func(G_GNUC_UNUSED voidpf opaque,
                          voidpf stream,
                          uLong offset,
                          int origin)
{
    return fseek((FILE*)stream, offset, origin);
}

G_GNUC_UNUSED
static unzFile
gwyminizip_unzOpen(const gchar *path)
{
    static zlib_filefunc_def ffdef = {
        gwyminizip_open_file_func,
        gwyminizip_read_file_func,
        gwyminizip_write_file_func,
        gwyminizip_tell_file_func,
        gwyminizip_seek_file_func,
        gwyminizip_close_file_func,
        gwyminizip_testerror_file_func,
        NULL,
    };

    return unzOpen2(path, &ffdef);
}
#else
#define gwyminizip_unzOpen unzOpen
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
                _("Minizip error while reading the zip file: %s."),
                errstr);
    return FALSE;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
