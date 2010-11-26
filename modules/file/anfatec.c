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
#define DEBUG 1
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
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC ";ANFATEC Parameterfile"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION_HEADER ".txt"
#define EXTENSION_DATA ".int"

#define Nanometer 1e-9

static gboolean      module_register           (void);
static gint          anfatec_detect            (const GwyFileDetectInfo *fileinfo,
                                                gboolean only_name);
static gchar*        anfatec_find_parameterfile(const gchar *filename);
static GwyContainer* anfatec_load              (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static GwyDataField* anfatec_load_channel      (GHashTable *hash,
                                                gint id,
                                                gchar **title);

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
    /* 4 is the length of .int, we start with removal of that. */
    guint len, removed = 4, ntries = 3;
    gboolean removed_something;

    if (g_str_has_suffix(filename, ".txt")
        || g_str_has_suffix(filename, ".TXT"))
        return g_strdup(filename);

    if (g_str_has_suffix(filename, ".int")
        || g_str_has_suffix(filename, ".INT")) {
        gwy_debug("File name ends with .int");
        paramfile = g_strdup(filename);
        len = strlen(paramfile);

        do {
            removed_something = FALSE;

            /* Try to add .txt, both lower- and uppercase */
            strcpy(paramfile + len-removed, ".txt");
            gwy_debug("Looking for %s", paramfile);
            if (g_file_test(paramfile, G_FILE_TEST_IS_REGULAR
                            || G_FILE_TEST_IS_SYMLINK)) {
                gwy_debug("Found.");
                return paramfile;
            }
            gwy_debug("Looking for %s", paramfile);
            strcpy(paramfile + len-removed, ".TXT");
            if (g_file_test(paramfile, G_FILE_TEST_IS_REGULAR
                            || G_FILE_TEST_IS_SYMLINK)) {
                gwy_debug("Found.");
                return paramfile;
            }

            /* Remove a contiguous sequence matching [A-Z]+[a-z]*.  This means
             * something like TopoFwd. */
            while (removed < len && g_ascii_islower(paramfile[len-removed-1])) {
                removed_something = TRUE;
                removed++;
            }
            while (removed < len && g_ascii_isupper(paramfile[len-removed-1])) {
                removed_something = TRUE;
                removed++;
            }
        } while (removed_something && removed < len && ntries--);

        gwy_debug("No matching paramter file.");
        g_free(paramfile);
    }

    return NULL;
}

static GwyContainer*
anfatec_load(const gchar *filename,
             GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    GHashTable *hash = NULL;
    gchar *line, *value, *key, *text = NULL;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gsize size;
    gint sectdepth, id, maxid;

    if (!g_file_get_contents(filename, &text, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!gwy_memmem(text, MIN(size, GWY_FILE_DETECT_BUFFER_SIZE),
                    MAGIC, MAGIC_SIZE)) {
        gchar *paramfile = anfatec_find_parameterfile(filename);

        /* If we are given data but find a suitable parameter file, recurse. */
        if (paramfile) {
            if (!gwy_strequal(paramfile, filename))
                container = anfatec_load(paramfile, mode, error);
            else
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_IO,
                            _("The parameter file cannot be loaded."));
            g_free(paramfile);
        }
        else
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_IO,
                        _("Cannot find the corresponding parameter file."));
        return container;
    }

    /* Cannot use GwyTextHeaderParser due to unlabelled sections. */
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    sectdepth = 0;
    id = -1;
    while ((line = gwy_str_next_line(&text))) {
        g_strstrip(line);
        if (!line[0] || line[0] == ';')
            continue;

        if (gwy_strequal(line, "FileDescBegin")) {
            if (sectdepth) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("FileDescBegin cannot be inside another "
                              "FileDesc."));
                goto fail;
            }
            sectdepth++;
            id++;
            continue;
        }
        if (gwy_strequal(line, "FileDescEnd")) {
            if (!sectdepth) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("FileDescEnd has no corresponding "
                              "FileDescBegin."));
                goto fail;
            }
            sectdepth--;
            continue;
        }

        if (!(value = strchr(line, ':'))) {
            g_warning("Cannot parse line %s", line);
            continue;
        }

        *value = '\0';
        value++;
        g_strchomp(line);
        g_strchug(value);
        if (sectdepth)
            key = g_strdup_printf("%d::%s", id, line);
        else
            key = g_strdup(line);
        gwy_debug("<%s> = <%s>", key, value);
        g_hash_table_replace(hash, key, value);
    }

    if (sectdepth) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("FileDescBegin has no corresponding "
                      "FileDescEnd."));
        goto fail;
    }

    if (id == -1) {
        err_NO_DATA(error);
        goto fail;
    }

    if (!require_keys(hash, error,
                      "xPixel", "yPixel",
                      "XScanRange", "YScanRange",
                      NULL))
        goto fail;

    maxid = id;
    for (id = 0; id <= maxid; id++) {
        GQuark quark;
        gchar *title;

        if (!(dfield = anfatec_load_channel(hash, id, &title)))
            continue;

        if (!container)
            container = gwy_container_new();
        quark = gwy_app_get_data_key_for_id(id);
        gwy_container_set_object(container, quark, dfield);

        if (title) {
            key = g_strdup_printf("/%d/data/title", id);
            gwy_container_set_string_by_name(container, key, g_strdup(title));
            g_free(key);
        }
        else
            gwy_app_channel_title_fall_back(container, id);
    }

    err_NO_DATA(error);
fail:
    g_free(text);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

static GwyDataField*
anfatec_load_channel(GHashTable *hash,
                     gint id,
                     gchar **title)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *unitx = NULL, *unity = NULL, *unitz = NULL;
    const gchar *filename;
    gint xres, yres, i, power10x, power10y, power10z;
    gdouble xreal, yreal, q;
    gint32 *buffer = NULL;
    gchar *key, *value;
    gdouble *data;
    FILE *fh;

    *title = NULL;

    xres = atoi(g_hash_table_lookup(hash, "xPixel"));
    yres = atoi(g_hash_table_lookup(hash, "xPixel"));
    if (err_DIMENSION(NULL, xres) || err_DIMENSION(NULL, yres))
        return NULL;

    key = g_strdup_printf("%d::FileName", id);
    filename = g_hash_table_lookup(hash, key);
    g_free(key);
    if (!filename) {
        g_warning("Missing FileName in channel %d.", id);
        return NULL;
    }

    /* Must use system fopen() that does no file name charset conversion. */
    fh = fopen(filename, "rb");
    if (!fh) {
        g_warning("Cannot open %s.", filename);
        goto fail;
    }

    buffer = g_new(gint32, xres*yres);
    if (fread(buffer, sizeof(guint32), xres*yres, fh) != xres*yres) {
        g_warning("Cannot read data from %s.", filename);
        goto fail;
    }
    fclose(fh);
    fh = NULL;

    xreal = g_ascii_strtod(g_hash_table_lookup(hash, "XScanRange"), NULL);
    xreal = fabs(xreal);
    if (!(xreal > 0.0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    yreal = g_ascii_strtod(g_hash_table_lookup(hash, "YScanRange"), NULL);
    if (!(yreal > 0.0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    unitx = gwy_si_unit_new_parse(g_hash_table_lookup(hash, "XPhysUnit"),
                                  &power10x);
    unity = gwy_si_unit_new_parse(g_hash_table_lookup(hash, "YPhysUnit"),
                                  &power10y);
    if (!gwy_si_unit_equal(unitx, unity))
        g_warning("X and Y units differ, using X");

    key = g_strdup_printf("%d::PhysUnit", id);
    unitz = gwy_si_unit_new_parse(g_hash_table_lookup(hash, key),
                                  &power10z);
    g_free(key);

    dfield = gwy_data_field_new(xres, yres,
                                xreal*pow10(power10x), yreal*pow10(power10y),
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, unitx);
    gwy_data_field_set_si_unit_z(dfield, unitz);

    q = pow10(power10z);
    key = g_strdup_printf("%d::Scale", id);
    if ((value = g_hash_table_lookup(hash, key)))
        q *= g_ascii_strtod(value, NULL);
    g_free(key);

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*GINT32_FROM_LE(buffer[i]);
    }

    key = g_strdup_printf("%d::Caption", id);
    if ((value = g_hash_table_lookup(hash, key)))
        *title = value;
    g_free(key);

fail:
    if (fh)
        fclose(fh);
    if (buffer)
        g_free(buffer);
    gwy_object_unref(unitx);
    gwy_object_unref(unity);
    gwy_object_unref(unitz);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
