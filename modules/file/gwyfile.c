/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register     (const gchar *name);
static gint          gwyfile_detect      (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* gwyfile_load        (const gchar *filename);
static gboolean      gwyfile_save        (GwyContainer *data,
                                          const gchar *filename);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "gwyfile",
    "Load and save Gwyddion native serialized objects.",
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo gwyfile_func_info = {
        "gwyfile",
        "Gwyddion native serialized objects (" EXTENSION ")",
        (GwyFileDetectFunc)&gwyfile_detect,
        (GwyFileLoadFunc)&gwyfile_load,
        (GwyFileSaveFunc)&gwyfile_save,
    };

    gwy_file_func_register(name, &gwyfile_func_info);

    return TRUE;
}

static gint
gwyfile_detect(const gchar *filename,
               gboolean only_name)
{
    FILE *fh;
    gchar magic[MAGIC_SIZE];
    gint score;

    if (only_name)
        return g_str_has_suffix(filename, EXTENSION) ? 20 : 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
gwyfile_load(const gchar *filename)
{
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < MAGIC_SIZE
        || memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        if (!gwy_file_abandon_contents(buffer, size, &err)) {
            g_critical("%s", err->message);
            g_clear_error(&err);
        }
        return NULL;
    }

    object = gwy_serializable_deserialize(buffer + MAGIC_SIZE,
                                          size - MAGIC_SIZE, &pos);
    if (!gwy_file_abandon_contents(buffer, size, &err)) {
        g_critical("%s", err->message);
        g_clear_error(&err);
    }
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }

    return (GwyContainer*)object;
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename)
{
    GByteArray *buffer;
    FILE *fh;
    gboolean ok = TRUE;

    if (!(fh = fopen(filename, "wb")))
        return FALSE;
    buffer = gwy_serializable_serialize(G_OBJECT(data), NULL);
    if (fwrite(MAGIC, 1, MAGIC_SIZE, fh) != MAGIC_SIZE
        || fwrite(buffer->data, 1, buffer->len, fh) != buffer->len) {
        ok = FALSE;
        unlink(filename);
    }
    fclose(fh);
    g_byte_array_free(buffer, TRUE);

    return ok;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
