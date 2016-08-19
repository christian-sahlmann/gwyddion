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

/* TODO: Add magic comments one the things is marginally working. */
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "\x5c\x26\x14\x00"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

enum {
    HEADER_SIZE = 0xa000,
    /* Could be the other way round.  I only have square images. */
    XRES_OFFSET = 0x0a90,
    YRES_OFFSET = 0x0aa8,
};

static gboolean      module_register(void);
static gint          dimfile_detect (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* dimfile_load   (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gchar**       find_images    (const guchar *buffer,
                                     guint size);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports old Veeco Dimension 3100D files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("dimensionfile",
                           N_("Dimension 3100D files (.001, .002, ...)"),
                           (GwyFileDetectFunc)&dimfile_detect,
                           (GwyFileLoadFunc)&dimfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
dimfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    const gchar *head = fileinfo->head;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len <= 64
        || fileinfo->file_size < HEADER_SIZE + 2
        || (memcmp(head, MAGIC, MAGIC_SIZE) != 0))
        return 0;

    gwy_debug("magic header OK");
    /* We have no idea what the binary header looks like.  But it has some
     * fields names as strings so look for them. */
    if (!gwy_memmem(head, fileinfo->buffer_len, "@Sens. ", strlen("@Sens. ")))
            return 0;
    /* The time. */
    if (!g_ascii_isdigit(head[38]) || !g_ascii_isdigit(head[39])
        || head[40] != ':'
        || !g_ascii_isdigit(head[41]) || !g_ascii_isdigit(head[42])
        || head[43] != ':'
        || !g_ascii_isdigit(head[44]) || !g_ascii_isdigit(head[45]))
        return FALSE;

    return 70;
}

static GwyContainer*
dimfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint xres, yres;
    gchar **images = NULL;
    guint nimages, i, imagesize;
    const guchar *p;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        goto fail;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Dimension");
        goto fail;
    }

    p = buffer + XRES_OFFSET;
    xres = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, xres))
        goto fail;

    p = buffer + YRES_OFFSET;
    yres = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, yres))
        goto fail;

    images = find_images(buffer, HEADER_SIZE);
    nimages = g_strv_length(images);
    imagesize = xres*yres*sizeof(guint16);
    if (err_SIZE_MISMATCH(error, HEADER_SIZE + nimages*imagesize, size, TRUE))
        goto fail;

    container = gwy_container_new();
    for (i = 0; i < nimages; i++) {
        dfield = gwy_data_field_new(xres, yres, xres, yres, FALSE);
        gwy_convert_raw_data(buffer + HEADER_SIZE + i*imagesize, xres*yres, 1,
                             GWY_RAW_DATA_SINT16, G_LITTLE_ENDIAN,
                             gwy_data_field_get_data(dfield), 1.0, 0.0);
        gwy_container_set_object(container,
                                 gwy_app_get_data_key_for_id(i), dfield);
        gwy_container_set_const_string(container,
                                       gwy_app_get_data_title_key_for_id(i),
                                       images[i]);
        gwy_app_channel_check_nonsquare(container, i);
        gwy_file_channel_import_log_add(container, i, NULL, filename);
    }

fail:
    g_strfreev(images);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gchar**
find_images(const guchar *buffer, guint size)
{
    static const guchar tag[] = "@Image Data\x00S\x00\x00\x00";
    GPtrArray *images = g_ptr_array_new();
    const guchar *p = buffer, *end;

    while ((p = gwy_memmem(p, size - (p - buffer), tag, sizeof(tag)-1))) {
        p += sizeof(tag)-1;
        end = memchr(p, '\x00', size - (p - buffer));
        if (!end || end == p)
            break;

        gwy_debug("Found image name <%s>", p);
        g_ptr_array_add(images, g_strdup(p));
        p += strlen(p);
    }

    g_ptr_array_add(images, NULL);

    return (gchar**)g_ptr_array_free(images, FALSE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
