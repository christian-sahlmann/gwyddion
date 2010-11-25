/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti)
 *  E-mail: yeti@gwyddion.net
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-anfatec-spm">
 *   <comment>Anfatec SPM data parameters</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0:160" value=";ANFATEC Parameterfile"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Anfatec
 * .par .int
 * Read
 **/

/* XXX: Take care to pass file names we get from GLib/Gtk+ to gstdio funcs
 * while raw file names to system stdio funcs. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC ";ANFATEC Parameterfile"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION_HEADER ".txt"
#define EXTENSION_DATA ".int"

#define Nanometer 1e-9

static gboolean      module_register            (void);
static gint          anfatec_detect             (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name);
static gchar* anfatec_find_parameterfile(const gchar *filename);
static GwyContainer* anfatec_load               (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Anfatec data files (two-part .txt + .int)."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("anfatec",
                           N_("Anfatec files (.par + .int)"),
                           (GwyFileDetectFunc)&anfatec_detect,
                           (GwyFileLoadFunc)&anfatec_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
anfatec_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    FILE *fh;
    gchar *parameterfile;
    gchar *buf;
    guint  size, result;

    if (only_name)
        return 0;

    if (strstr(fileinfo->head, MAGIC))
        return 90;

    if (!(parameterfile = anfatec_find_parameterfile(fileinfo->name)))
        return 0;

    fh = g_fopen(parameterfile, "r");
    if (!fh) {
        g_free(parameterfile);
        return 0;
    }
    buf = g_new(gchar, GWY_FILE_DETECT_BUFFER_SIZE);
    size = fread(buf, 1, GWY_FILE_DETECT_BUFFER_SIZE, fh);
    buf[MIN(GWY_FILE_DETECT_BUFFER_SIZE-1, size)] = '\0';
    result = strstr(fileinfo->head, MAGIC) != NULL;
    fclose(fh);
    g_free(buf);
    g_free(parameterfile);

    return result ? 90 : 0;
}

static gchar*
anfatec_find_parameterfile(const gchar *filename)
{
    gchar *paramfile;
    guint len;

    if (g_str_has_suffix(filename, ".int")
        || g_str_has_suffix(filename, ".INT")) {
        gwy_debug("File name ends with .int");
        paramfile = g_strdup(filename);
        len = strlen(paramfile);
        strcpy(paramfile + len-3, ".txt");
        gwy_debug("Looking for %s", paramfile);
        if (g_file_test(paramfile, G_FILE_TEST_IS_REGULAR
                        || G_FILE_TEST_IS_SYMLINK)) {
            gwy_debug("Found.");
            return paramfile;
        }
        gwy_debug("Looking for %s", paramfile);
        strcpy(paramfile + len-3, ".TXT");
        if (g_file_test(paramfile, G_FILE_TEST_IS_REGULAR
                        || G_FILE_TEST_IS_SYMLINK)) {
            gwy_debug("Found.");
            return paramfile;
        }
        g_free(paramfile);
        gwy_debug("No matching paramtert file.");
    }

    return NULL;
}

static GwyContainer*
anfatec_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL, *meta;
    gchar *text = NULL;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar key[32];
    guint i;

    if (!g_file_get_contents(filename, &text, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }


fail:
    g_free(text);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
