/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 * <mime-type type="application/x-alicona-imaging-al3d">
 *   <comment></comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="AliconaImaging\x00\x0d\x0a"/>
 *   </magic>
 *   <glob pattern="*.al3d"/>
 *   <glob pattern="*.AL3D"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Alicona Imaging AL3D data
 * .al3d
 * Read
 **/
#define DEBUG 1
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

#define MAGIC "AliconaImaging\x00\r\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".al3d"

enum {
    KEY_SIZE = 20,
    VALUE_SIZE = 30,
    CRLF_SIZE = 2,
    TAG_SIZE = KEY_SIZE + VALUE_SIZE + CRLF_SIZE,
    COMMENT_SIZE = 256,
    MIN_HEADER_SIZE = MAGIC_SIZE + 2*TAG_SIZE + COMMENT_SIZE,
};

typedef struct {
    gchar key[KEY_SIZE];
    gchar value[VALUE_SIZE];
    gchar crlf[CRLF_SIZE];
} Al3DTag;

typedef struct {
    const Al3DTag *version;
    const Al3DTag *counter;
    guint ntags;
    const Al3DTag *tags;
    const gchar *comment;
    const guchar *icon_data;
    const guchar *depth_data;
    const guchar *texture_data;
} Al3DFile;

static gboolean      module_register(void);
static gint          al3d_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* al3d_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean al3d_load_header(Al3DFile *afile,
                                 const guchar *buffer,
                                 gsize size,
                                 GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Alicona Imaging AL3D files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("alicona",
                           N_("Alicona Imaging AL3D files (.al3d)"),
                           (GwyFileDetectFunc)&al3d_detect,
                           (GwyFileLoadFunc)&al3d_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
al3d_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
al3d_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield = NULL, *mfield = NULL;
    Al3DFile afile;
    guchar *buffer = NULL;
    const gchar *unit, *title;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal;
    gint i, xres, yres, no_data_value = 32767;
    guint fi;
    gdouble *data, *mdata;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (!al3d_load_header(&afile, buffer, size, error))
        goto fail;

#if 0
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    yreal = 1.0;
    xreal = x_scale*yreal;
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);


    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), unit);

    mfield = gwy_data_field_new_alike(dfield, TRUE);
    data = gwy_data_field_get_data(dfield);
    mdata = gwy_data_field_get_data(mfield);

    for (i = 0; i < xres*yres; i++, p = end) {
        gint value = strtol(p, &end, 10);

        if (value != no_data_value && (type != CODEV_INT_INTENSITY_FILTER
                                       || value >= 0)) {
            mdata[i] = 1.0;
            data[i] = q*value;
        }
    }

    if (!gwy_app_channel_remove_bad_data(dfield, mfield))
        gwy_object_unref(mfield);

    container = gwy_container_new();

    /*
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    from F. Riguet : apparently no flip is needed (the raw data import module
    gives the correct orientation without further flipping)
    */
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);
    gwy_app_channel_check_nonsquare(container, 0);

    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup(title));

    if (mfield) {
        /*
        gwy_data_field_invert(mfield, FALSE, TRUE, FALSE);
        */
        gwy_container_set_object(container, gwy_app_get_mask_key_for_id(0),
                                 mfield);
        g_object_unref(mfield);
    }

    meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));
    gwy_container_set_string_by_name(meta, "Interpolation",
                                     g_strdup(nearest_neighbour
                                              ? "NNB" : "Linear"));
    gwy_container_set_string_by_name(meta, "Wavelength",
                                     g_strdup_printf("%g μm", wavelength));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);
#endif
    err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gboolean
check_tag(const Al3DTag *tag,
          GError **error)
{
    guint i;

    gwy_debug("tag <%.20s>", tag->key);
    if (tag->key[KEY_SIZE-1]) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header tag key is not nul-terminated."));
        return FALSE;
    }
    if (!tag->key[0]) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header tag key is empty."));
        return FALSE;
    }
    for (i = strlen(tag->key); i < KEY_SIZE-1; i++) {
        if (tag->key[i]) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Header tag ‘%s’ key is not nul-padded."),
                        tag->key);
            return FALSE;
        }
    }
    if (tag->crlf[0] != '\r' || tag->crlf[1] != '\n') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header tag ‘%s’ lacks CRLF terminator."),
                    tag->key);
        return FALSE;
    }
    if (tag->value[VALUE_SIZE-1]) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header tag ‘%s’ value is not nul-terminated."),
                    tag->key);
        return FALSE;
    }
    for (i = strlen(tag->value); i < VALUE_SIZE-1; i++) {
        if (tag->value[i]) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Header tag ‘%s’ value is not nul-padded."),
                        tag->key);
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
al3d_load_header(Al3DFile *afile,
                 const guchar *buffer,
                 gsize size,
                 GError **error)
{
    const guchar *p = buffer;
    gsize expected_size;
    guint i;

    gwy_clear(afile, 1);
    if (size < MIN_HEADER_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }
    if (memcmp(p, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Al3D");
        return FALSE;
    }
    p += MAGIC_SIZE;

    afile->version = (const Al3DTag*)p;
    p += TAG_SIZE;
    if (!check_tag(afile->version, error))
        return FALSE;
    if (!gwy_strequal(afile->version->key, "Version")) {
        err_MISSING_FIELD(error, "Version");
        return FALSE;
    }

    afile->counter = (const Al3DTag*)p;
    p += TAG_SIZE;
    if (!check_tag(afile->counter, error))
        return FALSE;
    if (!gwy_strequal(afile->counter->key, "TagCount")) {
        err_MISSING_FIELD(error, "TagCount");
        return FALSE;
    }

    afile->ntags = (guint)atoi(afile->counter->value);
    expected_size = TAG_SIZE*afile->ntags;
    if ((gsize)(size - (p - buffer)) < expected_size + COMMENT_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        return FALSE;
    }

    afile->tags = (const Al3DTag*)p;
    p += afile->ntags*TAG_SIZE;
    for (i = 0; i < afile->ntags; i++) {
        if (!check_tag(afile->tags + i, error))
            return FALSE;
    }

    afile->comment = p;
    if (afile->comment[COMMENT_SIZE-1] != '\n'
        || afile->comment[COMMENT_SIZE-2] != '\r') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Comment lacks CRLF termination."));
        return FALSE;
    }
    if (afile->comment[COMMENT_SIZE-3]) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Comment is not nul-terminated."));
        return FALSE;
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
