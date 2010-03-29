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
 * <mime-type type="application/x-gsf-spm">
 *   <comment>Gwyddion Simple Field data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Gwyddion Simple Field 1.0\n"/>
 *   </magic>
 *   <glob pattern="*.gsf"/>
 *   <glob pattern="*.GSF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion Simple Field
 * .gsf
 * Read Export
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "Gwyddion Simple Field 1.0\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".gsf"

static gboolean      module_register (void);
static gint          gsf_detect      (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* gsf_load        (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gboolean      gsf_export      (GwyContainer *data,
                                      const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static guint         read_pixel_size (GHashTable *hash,
                                      const gchar *key,
                                      GError **error);
static void          add_meta        (gpointer hkey,
                                      gpointer hvalue,
                                      gpointer user_data);
static gdouble       read_real_size  (GHashTable *hash,
                                      const gchar *key);
static gdouble       read_real_offset(GHashTable *hash,
                                      const gchar *key);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads and exports Gwyddion Simple Field files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gsffile",
                           N_("Gwyddion Simple Field (.gsf)"),
                           (GwyFileDetectFunc)&gsf_detect,
                           (GwyFileLoadFunc)&gsf_load,
                           NULL,
                           (GwyFileSaveFunc)&gsf_export);

    return TRUE;
}

static gint
gsf_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static GwyContainer*
gsf_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta = NULL;
    GwyDataField *dfield = NULL;
    GwyTextHeaderParser parser;
    GwySIUnit *unit;
    guchar *p, *value, *buffer = NULL, *header = NULL;
    const guchar *datap;
    GHashTable *hash = NULL;
    gsize size, expected_size;
    GError *err = NULL;
    gdouble xreal, yreal, xoff, yoff;
    guint i, xres, yres;
    gdouble *d;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Gwyddion Simple Field");
        goto fail;
    }

    p = buffer + MAGIC_SIZE;
    datap = memchr(p, '\0', size - (p - buffer));
    if (!datap) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        goto fail;
    }
    header = g_strdup(p);
    datap += 4 - ((datap - buffer) % 4);

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    if (!(hash = gwy_text_header_parse(header, &parser, NULL, NULL))) {
        g_propagate_error(error, err);
        goto fail;
    }

    xres = read_pixel_size(hash, "XRes", error);
    yres = read_pixel_size(hash, "YRes", error);
    if (!xres || !yres)
        goto fail;

    expected_size = (datap - buffer) + sizeof(gfloat)*xres*yres;
    if (err_SIZE_MISMATCH(error, expected_size, size, TRUE))
        goto fail;

    xreal = read_real_size(hash, "XReal");
    yreal = read_real_size(hash, "YReal");
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);

    xoff = read_real_offset(hash, "XOffset");
    yoff = read_real_offset(hash, "YOffset");
    gwy_data_field_set_xoffset(dfield, xoff);
    gwy_data_field_set_yoffset(dfield, yoff);

    value = g_hash_table_lookup(hash, "XYUnits");
    unit = gwy_si_unit_new(value);
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    value = g_hash_table_lookup(hash, "ZUnits");
    unit = gwy_si_unit_new(value);
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    d = gwy_data_field_get_data(dfield);
    for (i = xres*yres; i; i--)
        *(d++) = gwy_get_gfloat_le(&datap);

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if ((value = g_hash_table_lookup(hash, "Title"))) {
        /* FIXME: Ensure valid UTF-8 */
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));
    }
    else
        gwy_app_channel_title_fall_back(container, 0);

    meta = gwy_container_new();
    g_hash_table_foreach(hash, add_meta, meta);
    if (gwy_container_get_n_items(meta))
        gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (header)
        g_free(header);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

static void
add_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    if (gwy_stramong((gchar*)hkey,
                     "XRes", "YRes", "XReal", "YReal", "XOffset", "YOffset",
                     "XYUnits", "ZUnits", "Title",
                     NULL))
        return;

    gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                     (gchar*)hkey, g_strdup((gchar*)hvalue));
}

static guint
read_pixel_size(GHashTable *hash,
                const gchar *key,
                GError **error)
{
    gchar *value;
    guint size;

    if (!(value = g_hash_table_lookup(hash, key))) {
        err_MISSING_FIELD(error, key);
        return 0;
    }
    size = atoi(g_hash_table_lookup(hash, key));
    if (err_DIMENSION(error, size))
       return 0;

    return size;
}

static gdouble
read_real_size(GHashTable *hash,
               const gchar *key)
{
    gchar *value;
    gdouble dim = 1.0;

    if ((value = g_hash_table_lookup(hash, key))) {
        dim = g_ascii_strtod(value, NULL);
        /* Use negated positive conditions to catch NaNs */
        if (!((dim = fabs(dim)) > 0)) {
            g_warning("%s is 0.0 or NaN, fixing to 1.0", key);
            dim = 1.0;
        }
    }
    return dim;
}

static gdouble
read_real_offset(GHashTable *hash,
                 const gchar *key)
{
    gchar *value;
    gdouble off = 0.0;

    if ((value = g_hash_table_lookup(hash, key))) {
        off = g_ascii_strtod(value, NULL);
        /* Catch NaNs */
        if (!(off == off)) {
            g_warning("%s is NaN, fixing to 0.0", key);
            off = 0.0;
        }
    }
    return off;
}

static gboolean
gsf_export(GwyContainer *data,
           const gchar *filename,
           GwyRunType mode,
           GError **error)
{
    g_warning("Implement me!\n");
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
