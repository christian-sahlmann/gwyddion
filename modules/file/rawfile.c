/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

typedef enum {
    RAW_NONE = 0,
    RAW_BYTE,
    RAW_WORD,
    RAW_DOUBLEWORD,
    RAW_IEEE_FLOAT,
    RAW_IEEE_DOUBLE
} RawFileBuiltin;

/* note: size, skip, and rowskip are in bits */
typedef struct {
    RawFileBuiltin builtin;
    gsize offset;  /* offset from file start, in bytes */
    gsize size;  /* data sample size (unused if builtin) */
    gsize skip;  /* skip after each sample, except last (unused if builtin) */
    gsize rowskip;  /* skip after each sample row */
    gboolean sign;  /* take the number as signed? (unused if not integer) */
    gboolean revsam;  /* reverse bit order in samples? */
    gboolean revbyte;  /* reverse bit order in bytes? */
    gsize byteswap;  /* swap bytes (only for builtin) */
} RawFileSpec;

typedef struct {
    gsize xres;
    gsize yres;
    gdouble xreal;
    gdouble yreal;
    gdouble zscale;
} RawFileParams;

static gboolean      module_register     (const gchar *name);
static gint          rawfile_detect      (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* rawfile_load        (const gchar *filename);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "rawfile",
    "Read raw data according to user-specified format.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo rawfile_func_info = {
        "rawfile",
        "Raw data files",
        (GwyFileDetectFunc)&rawfile_detect,
        (GwyFileLoadFunc)&rawfile_load,
        NULL,
    };

    gwy_file_func_register(name, &rawfile_func_info);

    return TRUE;
}

static gint
rawfile_detect(const gchar *filename,
               gboolean only_name)
{
    FILE *fh;

    if (only_name)
        return 1;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    fclose(fh);

    return 1;
}

static GwyContainer*
rawfile_load(const gchar *filename)
{
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    /*
    if (!g_file_get_contents(filename, (gchar**)&buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < 4
        || memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        g_free(buffer);
        return NULL;
    }

    object = gwy_serializable_deserialize(buffer + 4, size - 4, &pos);
    g_free(buffer);
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }
    */

    return (GwyContainer*)object;
}

static void
rawfile_read_bits(RawFileSpec *spec,
                  RawFileParams *param,
                  guchar *buffer,
                  gdouble *data)
{
    gsize i, j, shift;
    guchar b, bucket;

    buffer += spec->offset;
    shift = 0;
    for (i = 0; i < param->yres; i++) {
        for (j = 0; j < param->xres; j++) {
        }
    }
}

static gsize
rawfile_compute_size(RawFileSpec *spec,
                     RawFileParams *param)
{
    gsize rowstride;

    rowstride = (spec->size + spec->skip)*param->xres
                + spec->rowskip - spec->skip;
    if (rowstride%8)
        g_warning("rowstride is not a whole number of bytes");
    rowstride = (rowstride + 7)%8;
    return spec->offset + param->yres*rowstride;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
