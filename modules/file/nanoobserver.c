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
#define EXTENSION ".nao"

static gboolean      module_register (void);
static gint          nao_detect      (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* nao_load        (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static GSList*       nao_file_list   (const gchar *filename);
static void          free_string_list(GSList *list);

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
    GSList *filelist = NULL, *l;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Generic ZIP file. */
    g_printerr("GENERIC ZIP\n");
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It contains directory Scan so this should be somewehre near the begining
     * of the file. */
    g_printerr("Scan/PK\n");
    if (!gwy_memmem(fileinfo->head, fileinfo->buffer_len, MAGIC1, MAGIC1_SIZE))
        return 0;

    /* We have to realy look inside. */
    g_printerr("FILELIST\n");
    if (!(filelist = nao_file_list(fileinfo->name)))
        return 0;

    for (l = filelist; l; l = g_slist_next(l)) {
        g_printerr("<%s>\n", (gchar*)l->data);
    }

    return 100;
}

static GwyContainer*
nao_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    err_NO_DATA(error);
    return container;
}

static GSList*
nao_file_list(const gchar *filename)
{
    GSList *filelist = NULL;
    unzFile zipfile;
    gint status;

    zipfile = unzOpen(filename);
    if (!zipfile)
        return NULL;

    status = unzGoToFirstFile(zipfile);
    while (status == UNZ_OK) {
        gchar filename_buf[PATH_MAX+1];
        if (unzGetCurrentFileInfo(zipfile, NULL, filename_buf, PATH_MAX,
                                  NULL, 0, NULL, 0) != UNZ_OK) {
            free_string_list(filelist);
            filelist = NULL;
            goto fail;
        }
        filelist = g_slist_prepend(filelist, g_strdup(filename_buf));
        status = unzGoToNextFile(zipfile);
    }
    filelist = g_slist_reverse(filelist);

fail:
    unzClose(zipfile);
    return filelist;
}

static void
free_string_list(GSList *list)
{
    GSList *l;

    for (l = list; l; l = g_slist_next(l))
        g_free(l->data);
    g_slist_free(list);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
