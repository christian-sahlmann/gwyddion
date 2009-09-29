/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 * <mime-type type="application/x-nanoscan-spm">
 *   <comment>NanoScan SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="&lt;?xml">
 *       <match type="string" offset="40:120" value="xmlns=\"http://www.swissprobe.com/SPM\""/>
 *     </match>
 *   </magic>
 * </mime-type>
 **/

#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC1 "<?xml"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 "<scan"
#define MAGIC3 "xmlns=\"http://www.swissprobe.com/SPM\""

#define EXTENSION ".xml"

#if GLIB_CHECK_VERSION(2, 12, 0)
#define TREAT_CDATA_AS_TEXT G_MARKUP_TREAT_CDATA_AS_TEXT
#else
#define TREAT_CDATA_AS_TEXT 0
#endif

typedef struct {
    GString *path;
    gchar *xyunits;
    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
} NanoScanFile;

static gboolean      module_register(void);
static gint          nanoscan_detect(const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* nanoscan_load  (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static void          start_element  (GMarkupParseContext *context,
                                     const gchar *element_name,
                                     const gchar **attribute_names,
                                     const gchar **attribute_values,
                                     gpointer user_data,
                                     GError **error);
static void          end_element    (GMarkupParseContext *context,
                                     const gchar *element_name,
                                     gpointer user_data,
                                     GError **error);
static void          text           (GMarkupParseContext *context,
                                     const gchar *text,
                                     gsize text_len,
                                     gpointer user_data,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NanoScan XML files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoscan",
                           N_("NanoScan XML files (.xml)"),
                           (GwyFileDetectFunc)&nanoscan_detect,
                           (GwyFileLoadFunc)&nanoscan_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gboolean
check_magic(const gchar *header)
{
    return (memcmp(header, MAGIC1, MAGIC1_SIZE) == 0
            && strstr(header, MAGIC2) != NULL
            && strstr(header, MAGIC3) != NULL);
}

static gint
nanoscan_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    return check_magic(fileinfo->head) ? 100 : 0;
}

static GwyContainer*
nanoscan_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta;
    NanoScanFile nfile;
    /*
    GwyDataField *dfield = NULL;
    GwySIUnit *unit;
    */
    gchar *head, *buffer = NULL;
    GMarkupParser parser = { start_element, end_element, text, NULL, NULL };
    GMarkupParseContext *context;
    gsize size;
    GError *err = NULL;
    gdouble q;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    gwy_clear(&nfile, 1);
    head = g_strndup(buffer, 1024);
    if (!check_magic(head)) {
        err_FILE_TYPE(error, "NanoScan XML");
        g_free(head);
        goto fail;
    }
    g_free(head);

    nfile.path = g_string_new(NULL);
    context = g_markup_parse_context_new(&parser,
                                         G_MARKUP_PREFIX_ERROR_POSITION
                                         | TREAT_CDATA_AS_TEXT,
                                         &nfile, NULL);
    if (!g_markup_parse_context_parse(context, buffer, size, error)) {
        g_markup_parse_context_free(context);
        goto fail;
    }
    g_markup_parse_context_free(context);


fail:
    g_free(buffer);
    if (nfile.path)
        g_string_free(nfile.path, TRUE);

    return container;
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              G_GNUC_UNUSED const gchar **attribute_names,
              G_GNUC_UNUSED const gchar **attribute_values,
              gpointer user_data,
              GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;

    if (!nfile->path->len && !gwy_strequal(element_name, "scan")) {
        /* TODO: Make big noise. */
    }

    g_string_append_c(nfile->path, '/');
    g_string_append(nfile->path, element_name);
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;
    gchar *pos;

    pos = memrchr(nfile->path->str, '/', nfile->path->len);
    /* GMarkupParser should raise a run-time error if this does not hold. */
    g_assert(pos && strcmp(pos + 1, element_name) == 0);
    g_string_truncate(nfile->path, pos - nfile->path->str);
}

#define RES_PREFIX "/scan/vector/contents/size/contents"
#define RES_PREFIX_SIZE (sizeof(RES_PREFIX)-1)

#define DIMS_PREFIX "/scan/vector/contents/area/contents"
#define DIMS_PREFIX_SIZE (sizeof(DIMS_PREFIX)-1)

static void
text(G_GNUC_UNUSED GMarkupParseContext *context,
     const gchar *value,
     gsize value_len,
     gpointer user_data,
     G_GNUC_UNUSED GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;
    const gchar *path = nfile->path->str;

    if (strncmp(path, RES_PREFIX, RES_PREFIX_SIZE) == 0) {
        path += RES_PREFIX_SIZE;
        if (gwy_strequal(path, "/fast_axis/v")) {
            nfile->xres = atoi(value);
            gwy_debug("xres: %d", nfile->xres);
        }
        else if (gwy_strequal(path, "/slow_axis/v")) {
            nfile->yres = atoi(value);
            gwy_debug("xres: %d", nfile->yres);
        }
    }
    else if (strncmp(path, DIMS_PREFIX, DIMS_PREFIX_SIZE) == 0) {
        path += RES_PREFIX_SIZE;
        if (gwy_strequal(path, "/unit/v")) {
            g_free(nfile->xyunits);
            nfile->xyunits = g_strdup(value);
            gwy_debug("xyunits: %s", value);
        }
        else if (gwy_strequal(path, "/size/contents/fast_axis/v")) {
            nfile->xreal = g_ascii_strtod(value, NULL);
            gwy_debug("xreal: %g", nfile->xreal);
        }
        else if (gwy_strequal(path, "/size/contents/slow_axis/v")) {
            nfile->yreal = g_ascii_strtod(value, NULL);
            gwy_debug("yreal: %g", nfile->yreal);
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
