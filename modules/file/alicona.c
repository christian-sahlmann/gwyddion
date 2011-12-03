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
    ICON_SIZE = 68400,
};

typedef struct {
    gchar key[KEY_SIZE];
    gchar value[VALUE_SIZE];
    gchar crlf[CRLF_SIZE];
} Al3DTag;

typedef struct {
    const Al3DTag *version;
    const Al3DTag *counter;
    const Al3DTag *tags;
    const gchar *comment;
    const guchar *icon_data;
    const guchar *depth_data;
    const guchar *texture_data;
    /* Cached parsed values. */
    guint ntags;
    guint xres;
    guint yres;
    guint nplanes;
    gdouble dx;
    gdouble dy;
    guint iconoffset;
    guint textureoffset;
    guint depthoffset;
} Al3DFile;

static gboolean       module_register   (void);
static gint           al3d_detect       (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer*  al3d_load         (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static void           set_title         (GwyContainer *container,
                                         guint id,
                                         const gchar *name,
                                         gint component);
static void           add_meta          (GwyContainer *container,
                                         guint id,
                                         const Al3DFile *afile);
static gchar*         texture_lo_ptr    (const Al3DTag *tag);
static gchar*         texture_ptr       (const Al3DTag *tag);
static GwyDataField*  read_depth_image  (const Al3DFile *afile,
                                         const guchar *buffer,
                                         GwyDataField **badmask);
static GwyDataField*  read_hi_lo_texture(const Al3DFile *afile,
                                         const Al3DTag *hitag,
                                         const Al3DTag *lotag,
                                         const guchar *buffer,
                                         GError **error);
static GwyDataField*  read_texture      (const Al3DFile *afile,
                                         const Al3DTag *tag,
                                         const guchar *buffer,
                                         GError **error);
static gboolean       al3d_load_header  (Al3DFile *afile,
                                         const guchar *buffer,
                                         gsize size,
                                         GError **error);
static const Al3DTag* find_tag          (const Al3DFile *afile,
                                         const gchar *name,
                                         GError **error);
static gboolean       read_uint_tag     (const Al3DFile *afile,
                                         const gchar *name,
                                         guint *retval,
                                         GError **error);
static gboolean       read_float_tag    (const Al3DFile *afile,
                                         const gchar *name,
                                         gdouble *retval,
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
    GwyContainer *container = NULL;
    GwyDataField *field = NULL;
    Al3DFile afile;
    guchar *buffer = NULL;
    guint firstfreepos;
    gsize size;
    GError *err = NULL;
    guint i, id = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (!al3d_load_header(&afile, buffer, size, error))
        goto fail;

    if (!read_uint_tag(&afile, "Cols", &afile.xres, error)
        || !read_uint_tag(&afile, "Rows", &afile.yres, error))
        goto fail;

    if (err_DIMENSION(error, afile.xres) || err_DIMENSION(error, afile.yres))
        goto fail;

    if (!read_float_tag(&afile, "PixelSizeXMeter", &afile.dx, error)
        || !read_float_tag(&afile, "PixelSizeYMeter", &afile.dy, error))
        goto fail;

    if (!((afile.dx = fabs(afile.dx)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        afile.dx = 1.0/afile.xres;
    }
    if (!((afile.dy = fabs(afile.dy)) > 0)) {
        g_warning("Real y step is 0.0, fixing to 1.0");
        afile.dy = 1.0/afile.yres;
    }

    firstfreepos = MAGIC_SIZE + (TAG_SIZE + 2)*afile.ntags + COMMENT_SIZE;
    read_uint_tag(&afile, "IconOffset", &afile.iconoffset, NULL);
    if (afile.iconoffset) {
        if (afile.iconoffset < firstfreepos
            || size < firstfreepos + ICON_SIZE
            || afile.iconoffset > size - ICON_SIZE) {
            err_INVALID(error, "IconOffset");
            goto fail;
        }
        firstfreepos += ICON_SIZE;
    }

    read_uint_tag(&afile, "DepthImageOffset", &afile.depthoffset, NULL);
    if (afile.depthoffset) {
        guint rowstride = (afile.xres*sizeof(gfloat) + 7)/8*8;
        guint imagesize = afile.yres*rowstride;
        if (afile.depthoffset < firstfreepos
            || size < firstfreepos + imagesize
            || afile.depthoffset > size - imagesize) {
            err_INVALID(error, "DepthImageOffset");
            goto fail;
        }
        firstfreepos += imagesize;
    }

    read_uint_tag(&afile, "NumberOfPlanes", &afile.nplanes, NULL);
    read_uint_tag(&afile, "TextureImageOffset", &afile.textureoffset, NULL);
    if (afile.nplanes && afile.textureoffset) {
        guint rowstride = (afile.xres + 7)/8*8;
        guint planesize = afile.yres*rowstride;
        if (afile.textureoffset < firstfreepos
            || size < firstfreepos + planesize*afile.nplanes
            || afile.textureoffset > size - planesize*afile.nplanes) {
            err_INVALID(error, "TextureImageOffset");
            goto fail;
        }
        firstfreepos += planesize*afile.nplanes;
    }

    if (firstfreepos
        == MAGIC_SIZE + (TAG_SIZE + 2)*afile.ntags + COMMENT_SIZE) {
        err_NO_DATA(error);
        goto fail;
    }

    container = gwy_container_new();

    if (afile.depthoffset) {
        GwyDataField *badmask = NULL;

        field = read_depth_image(&afile, buffer, &badmask);
        gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                 field);
        if (badmask) {
            gwy_app_channel_remove_bad_data(field, badmask);
            gwy_container_set_object(container, gwy_app_get_mask_key_for_id(id),
                                     badmask);
            g_object_unref(badmask);
        }
        g_object_unref(field);
        set_title(container, id, "Depth", -1);
        add_meta(container, id, &afile);
        id++;
    }

    for (i = 0; i < afile.ntags; i++) {
        const Al3DTag *tag = afile.tags + i;
        gchar *name;

        if ((name = texture_lo_ptr(tag))) {
            gchar *hikey = gwy_strreplace(tag->key, "LoPtr", "HiPtr", 1);
            const Al3DTag *hitag = find_tag(&afile, hikey, NULL);
            g_free(hikey);

            gwy_debug("loptr tag <%s> (%s) = %s", tag->key, name, tag->value);
            field = read_hi_lo_texture(&afile, hitag, tag, buffer, &err);
            if (!field) {
                g_warning("%s", err->message);
                g_clear_error(&err);
                g_free(name);
                continue;
            }
            gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                     field);
            set_title(container, id, name, -1);
            add_meta(container, id, &afile);
            g_free(name);
            id++;
        }
        else if ((name = texture_ptr(tag))) {
            gchar **planes = g_strsplit(tag->value, ";", 0);
            guint nplanes = g_strv_length(planes);
            guint j;

            gwy_debug("ptr tag <%s> (%s) = %s", tag->key, name, tag->value);
            for (j = 0; planes[j]; j++) {
                field = read_texture(&afile, tag, buffer, &err);
                if (!field) {
                    g_warning("%s", err->message);
                    g_clear_error(&err);
                    continue;
                }
                gwy_container_set_object(container,
                                         gwy_app_get_data_key_for_id(id),
                                         field);
                set_title(container, id, name, nplanes > 1 ? j : -1);
                add_meta(container, id, &afile);
                id++;
            }
            g_strfreev(planes);
            g_free(name);
        }
    }

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (container && !gwy_container_get_n_items(container)) {
        g_object_unref(container);
        container = NULL;
    }

    return container;
}

static void
set_title(GwyContainer *container,
          guint id,
          const gchar *name,
          gint component)
{
    gchar *title;
    gchar key[32];

    g_snprintf(key, sizeof(key), "/%u/data/title", id);
    if (component == -1)
        title = g_strdup(name);
    else if (component == 0)
        title = g_strdup_printf("%s (R)", name);
    else if (component == 1)
        title = g_strdup_printf("%s (G)", name);
    else if (component == 2)
        title = g_strdup_printf("%s (B)", name);
    else
        title = g_strdup_printf("%s (%u)", name, component);

    gwy_container_set_string_by_name(container, key, title);
}

static void
add_meta(GwyContainer *container,
         guint id,
         const Al3DFile *afile)
{
    GwyContainer *meta = gwy_container_new();
    gchar key[32];
    guint i;

    gwy_container_set_string_by_name(meta, afile->version->key,
                                     g_strdup(afile->version->value));
    for (i = 0; i < afile->ntags; i++) {
        const Al3DTag *tag = afile->tags + i;

        if (gwy_stramong(tag->key,
                         "DirSpacer", "PlaceHolder",
                         "Cols", "Rows", "NumberOfPlanes", "ImageCode",
                         "PixelSizeXMeter", "PixelSizeYMeter",
                         "InvalidPixelValue", NULL)
            || strstr(tag->key, "Ptr")
            || g_str_has_suffix(tag->key, "Offset"))
            continue;

        gwy_container_set_string_by_name(meta, tag->key, g_strdup(tag->value));
    }

    if (strlen(afile->comment)) {
        gchar *p = g_convert(afile->comment, strlen(afile->comment),
                             "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        if (p)
            gwy_container_set_string_by_name(meta, "Comment", p);
    }

    g_snprintf(key, sizeof(key), "/%u/meta", id);
    gwy_container_set_object_by_name(container, key, meta);
    g_object_unref(meta);
}

static gchar*
texture_lo_ptr(const Al3DTag *tag)
{
    const gchar *p = strstr(tag->key, "LoPtr");

    if (!p || p == tag->key)
        return NULL;

    return gwy_strreplace(tag->key, "LoPtr", "", 1);
}

static gchar*
texture_ptr(const Al3DTag *tag)
{
    const gchar *p = strstr(tag->key, "Ptr");

    if (!p || p == tag->key)
        return NULL;

    if (strstr(tag->key, "LoPtr") || strstr(tag->key, "HiPtr"))
        return NULL;

    return gwy_strreplace(tag->key, "Ptr", "", 1);
}

static GwyDataField*
read_depth_image(const Al3DFile *afile,
                 const guchar *buffer,
                 GwyDataField **badmask)
{
    GwyDataField *field, *mask = NULL;
    guint rowstride = (afile->xres*sizeof(gfloat) + 7)/8*8;
    gdouble *d, *m = NULL;
    guint i, j;
    gdouble invalid_value = 0.0/0.0;
    guint xres = afile->xres, yres = afile->yres;

    read_float_tag(afile, "InvalidPixelValue", &invalid_value, NULL);

    field = gwy_data_field_new(xres, yres, afile->dx*xres, afile->dy*yres,
                               FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(field), "m");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(field), "m");

    d = field->data;
    for (i = 0; i < yres; i++) {
        const guchar *p = buffer + afile->depthoffset + i*rowstride;

        for (j = 0; j < xres; j++, d++, m++) {
            *d = gwy_get_gfloat_le(&p);
            if (*d == invalid_value) {
                if (!mask) {
                    mask = gwy_data_field_new_alike(field, FALSE);
                    gwy_data_field_fill(mask, 1.0);
                    m = mask->data + i*xres + j;
                }
                *d = 0.0;
                *m = 0.0;
            }
        }
    }

    *badmask = mask;

    return field;
}

static gboolean
check_plane_no(const Al3DFile *afile,
               const gchar *key,
               guint planeno,
               GError **error)
{
    if (planeno < afile->nplanes)
        return TRUE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Invalid plane number %u in tag ‘%s’."),
                planeno, key);
    return FALSE;
}

static GwyDataField*
read_hi_lo_texture(const Al3DFile *afile,
                   const Al3DTag *hitag,
                   const Al3DTag *lotag,
                   const guchar *buffer,
                   GError **error)
{
    GwyDataField *field;
    guint xres = afile->xres, yres = afile->yres;
    guint rowstride = (afile->xres + 7)/8*8;
    guint hiplaneno = (guint)atoi(hitag->value);
    guint loplaneno = (guint)atoi(lotag->value);
    guint planesize = yres*rowstride;
    gdouble *d;
    guint i, j;

    if (!check_plane_no(afile, hitag->key, hiplaneno, error)
        || !check_plane_no(afile, lotag->key, loplaneno, error))
        return NULL;

    field = gwy_data_field_new(xres, yres, afile->dx*xres, afile->dy*yres,
                               FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(field), "m");

    d = field->data;

    for (i = 0; i < yres; i++) {
        const guchar *phi = (buffer + afile->textureoffset
                             + hiplaneno*planesize + i*rowstride);
        const guchar *plo = (buffer + afile->textureoffset
                             + loplaneno*planesize + i*rowstride);

        for (j = 0; j < xres; j++, d++, plo++, phi++)
            *d = (*plo | ((guint)*phi << 8))/65536.0;
    }

    return field;
}

static GwyDataField*
read_texture(const Al3DFile *afile,
             const Al3DTag *tag,
             const guchar *buffer,
             GError **error)
{
    GwyDataField *field;
    guint xres = afile->xres, yres = afile->yres;
    guint rowstride = (afile->xres + 7)/8*8;
    guint planeno = (guint)atoi(tag->value);
    guint planesize = yres*rowstride;
    gdouble *d;
    guint i, j;

    if (!check_plane_no(afile, tag->key, planeno, error))
        return NULL;

    field = gwy_data_field_new(xres, yres, afile->dx*xres, afile->dy*yres,
                               FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(field), "m");

    d = field->data;

    for (i = 0; i < yres; i++) {
        const guchar *p = (buffer + afile->textureoffset
                           + planeno*planesize + i*rowstride);

        for (j = 0; j < xres; j++, d++, p++)
            *d = *p/256.0;
    }

    return field;
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

static const Al3DTag*
find_tag(const Al3DFile *afile,
         const gchar *name,
         GError **error)
{
    guint i;

    if (gwy_strequal(name, "Version"))
        return afile->version;
    if (gwy_strequal(name, "TagCount"))
        return afile->counter;

    for (i = 0; i < afile->ntags; i++) {
        if (gwy_strequal(afile->tags[i].key, name))
            return afile->tags + i;
    }

    err_MISSING_FIELD(error, name);
    return NULL;
}

static gboolean
read_uint_tag(const Al3DFile *afile,
              const gchar *name,
              guint *retval,
              GError **error)
{
    const Al3DTag *tag;

    if (!(tag = find_tag(afile, name, error)))
        return FALSE;

    *retval = (guint)atol(tag->value);
    return TRUE;
}

static gboolean
read_float_tag(const Al3DFile *afile,
               const gchar *name,
               gdouble *retval,
               GError **error)
{
    const Al3DTag *tag;

    if (!(tag = find_tag(afile, name, error)))
        return FALSE;

    *retval = g_ascii_strtod(tag->value, NULL);
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
