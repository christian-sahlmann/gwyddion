/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-mapvue">
 *   <comment>MapVue data file</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x57\x04\x02\x00"/>
 *   </magic>
 *   <glob pattern="*.map"/>
 *   <glob pattern="*.MAP"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

/* This is actually image type (dec 1111) + the first reference tag (dec 2) */
#define MAGIC "\x57\x04\x02\x00"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".map"

enum {
    TAG_MARKER2 = 0xccaa,
    TAG_MARKER4 = 0xccbb,
};

typedef guint (*MapVueReadGroup)(const guchar **p,
                                 gsize size,
                                 gpointer group,
                                 GError **error);

typedef struct {
    gint reftag;
    gint column_start;
    gint row_start;
    gint n_columns;
    gint n_rows;
} MapVueGroup2;

typedef struct {
    gint reftag;
    gint tagmark;
    gint tagsize;
    gint column_start;
    gint row_start;
    gint n_columns;
    gint n_rows;
} MapVueGroup3;

typedef struct {
    gint reftag;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
    gchar field5[256];
    gchar time[9];
    gchar date[9];
} MapVueGroup52;

typedef struct {
    gint reftag;
    gint tagmark;
    gint tagsize;
    gchar field6[256];
    gchar field7[256];
} MapVueGroup53;

typedef struct {
    gint reftag;
    gdouble wedge;
    gdouble testwedge;
    gdouble wavelength;
    gdouble new_wavelength;
} MapVueGroup101;

typedef struct {
    gint reftag;
    gdouble data_scale_factor;
} MapVueGroup201;

typedef struct {
    gint reftag;
    gint n_segments;
} MapVueGroup451;

typedef struct {
    gint reftag;
    gint data_type;
} MapVueGroup501;

typedef struct {
    gint reftag;
    gdouble magnification;
    gdouble x_frame_scale;
    gdouble y_optical_scale;
} MapVueGroup551;

typedef struct {
    gint reftag;
    gint maximum_frames;
    gint display_rows;
    gint total_rows;
    gint total_columns;
} MapVueGroup651;

typedef struct {
    gint reftag;
    gint tag_mark;
    gint tag_size;
    gint system_information;
} MapVueGroup801;

typedef struct {
    gint reftag;
    gint tag_mark;
    gint tag_size;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
    gchar field5[256];
    gchar field6[256];
} MapVueGroup852;

typedef struct {
    gint reftag;
    gint tag_mark;
    gint tag_size;
    gchar comment[256];
} MapVueGroup901;

typedef struct {
    gint reftag;
    gint tag_mark;
    gint tag_size;
    gdouble stage_x;
    gdouble stage_y;
    gdouble stage_z;
} MapVueGroup1152;

typedef struct {
    gint id;
    MapVueGroup2 group2;
    MapVueGroup3 group3;
    MapVueGroup52 group52;
    MapVueGroup53 group53;
    MapVueGroup101 group101;
    MapVueGroup201 group201;
    MapVueGroup451 group451;
    MapVueGroup501 group501;
    MapVueGroup551 group551;
    MapVueGroup651 group651;
    MapVueGroup801 group801;
    MapVueGroup852 group852;
    MapVueGroup901 group901;
    MapVueGroup1152 group1152;
} MapVueFile;

static gboolean      module_register       (void);
static gint          mapvue_detect         (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* mapvue_load           (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static gboolean      mapvue_read_header    (gchar *buffer,
                                            MapVueFile *ufile,
                                            GError **error);
static GwyDataField* mapvue_read_data_field(const guchar *buffer,
                                            gsize size,
                                            MapVueFile *ufile,
                                            GError **error);
static GwyContainer* mapvue_get_metadata   (MapVueFile *ufile);
static guint         mapvue_skip_group     (const guchar **p,
                                            gsize size,
                                            guint reftag,
                                            GError **error);
static guint         mapvue_read_group2    (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group3    (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group52   (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group53   (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group101  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group201  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group451  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group501  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group551  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group651  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group801  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group852  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);
static guint         mapvue_read_group901  (const guchar **p,
                                            gsize size,
                                            gpointer grpdata,
                                            GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports MapVue data files (.map)."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("mapvue",
                           N_("MapVue files (.map)"),
                           (GwyFileDetectFunc)&mapvue_detect,
                           (GwyFileLoadFunc)&mapvue_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mapvue_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION)
               ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
mapvue_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    MapVueReadGroup readgroup;
    MapVueFile mapvuefile;
    gpointer groupdata;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *data_name;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "MapVue");
        goto fail;
    }

    p = buffer + 2;
    while ((gsize)(p - buffer) + 2 < size) {
        guint tagsize, reftag = gwy_get_guint16_le(&p);

        g_printerr("Found tag %u\n", reftag);
        /* The groups should be ordered, but do not rely on that. */
        if (reftag == 2) {
            readgroup = &mapvue_read_group2;
            groupdata = &mapvuefile.group2;
        }
        else if (reftag == 3) {
            readgroup = &mapvue_read_group3;
            groupdata = &mapvuefile.group3;
        }
        else if (reftag == 52) {
            readgroup = &mapvue_read_group52;
            groupdata = &mapvuefile.group52;
        }
        else if (reftag == 53) {
            readgroup = &mapvue_read_group53;
            groupdata = &mapvuefile.group53;
        }
        else if (reftag == 101) {
            readgroup = &mapvue_read_group101;
            groupdata = &mapvuefile.group101;
        }
        else if (reftag == 201) {
            readgroup = &mapvue_read_group201;
            groupdata = &mapvuefile.group201;
        }
        else if (reftag == 451) {
            readgroup = &mapvue_read_group451;
            groupdata = &mapvuefile.group451;
        }
        else if (reftag == 501) {
            readgroup = &mapvue_read_group501;
            groupdata = &mapvuefile.group501;
        }
        else if (reftag == 551) {
            readgroup = &mapvue_read_group551;
            groupdata = &mapvuefile.group551;
        }
        else if (reftag == 651) {
            readgroup = &mapvue_read_group651;
            groupdata = &mapvuefile.group651;
        }
        else if (reftag == 801) {
            readgroup = &mapvue_read_group801;
            groupdata = &mapvuefile.group801;
        }
        else if (reftag == 852) {
            readgroup = &mapvue_read_group852;
            groupdata = &mapvuefile.group852;
        }
        else if (reftag == 901) {
            readgroup = &mapvue_read_group901;
            groupdata = &mapvuefile.group901;
        }
        else {
            g_printerr("Unknown tag %u\n", reftag);
            tagsize = mapvue_skip_group(&p, size - (p - buffer), reftag, error);
            if (!tagsize)
                goto fail;

            g_printerr("Unknown group %u successfully skipped\n", reftag);
            continue;
        }

        tagsize = readgroup(&p, size - (p - buffer), groupdata, error);
        if (!tagsize)
            goto fail;

        g_printerr("Read group %u of size %u\n", reftag, tagsize);
    }

    /*
    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

    meta = mapvue_get_metadata(&ufile);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    mapvue_file_free(&ufile);
    */

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static inline gboolean
err_TAG_SIZE(GError **error, guint reftag, guint expected, guint real)
{
    if (expected < real)
        return FALSE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Expected tag %u size is %u bytes, "
                  "but the actual size is %u bytes."),
                reftag, expected, real);
    return TRUE;
}

/* A group with marker and tagsize -- which exlcudes reftag, marker and self. */
static guint
mapvue_group_size(const guchar **p, gsize size, guint reftag, GError **error)
{
    guint marker, sizesize, tagsize;

    if (size < 2) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag %u size is %u bytes, "
                      "which is not enough to hold the tag marker."),
                    reftag, (guint)size);
        return 0;
    }

    marker = gwy_get_guint16_le(p);
    if (marker == TAG_MARKER2) {
        if (size < 4) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Tag %u size is %u bytes, "
                          "which is not enough to hold "
                          "the tag marker and size."),
                        reftag, (guint)size);
            return 0;
        }
        tagsize = gwy_get_guint16_le(p);
        sizesize = 2;
    }
    else if (marker == TAG_MARKER4) {
        if (size < 6) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Tag %u size is %u bytes, "
                          "which is not enough to hold "
                          "the tag marker and size."),
                        reftag, (guint)size);
            return 0;
        }
        tagsize = gwy_get_guint32_le(p);
        sizesize = 4;
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag marker is missing on an unknown tag %u."),
                    reftag);
        return 0;
    }

    if (err_TAG_SIZE(error, reftag, tagsize + 4, size))
        return 0;

    return tagsize + 2 + sizesize;
}

/* Read pascal-like string with size checking. */
static guint
mapvue_read_string(const guchar **p, gsize size, gchar *str, GError **error)
{
    guint strsize;

    if (size < 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Character array does not fit into the file."));
        return 0;
    }

    strsize = **p;
    (*p)++;

    if (strsize+1 > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Character array does not fit into the file."));
        return 0;
    }

    memcpy(str, *p, strsize);
    str[strsize] = '\0';
    *p += strsize;
    return strsize + 1;
}

static guint
mapvue_skip_group(const guchar **p, gsize size, guint reftag, GError **error)
{
    const guchar *porig = *p;
    guint tagsize = mapvue_group_size(p, size, reftag, error);

    if (!tagsize)
        return 0;

    *p = porig + tagsize;

    return tagsize;
}

static guint
mapvue_read_group2(const guchar **p, gsize size, gpointer grpdata,
                   GError **error)
{
    enum { SIZE = 8 };
    MapVueGroup2 *group = (MapVueGroup2*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->column_start = gwy_get_guint16_le(p);
    group->row_start = gwy_get_guint16_le(p);
    group->n_columns = gwy_get_guint16_le(p);
    group->n_rows = gwy_get_guint16_le(p);
    return SIZE;
}

static guint
mapvue_read_group3(const guchar **p, gsize size, gpointer grpdata,
                   GError **error)
{
    MapVueGroup3 *group = (MapVueGroup3*)grpdata;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(p, size, group->reftag, error)))
        return 0;

    group->column_start = gwy_get_guint32_le(p);
    group->row_start = gwy_get_guint32_le(p);
    group->n_columns = gwy_get_guint32_le(p);
    group->n_rows = gwy_get_guint32_le(p);
    return size;
}

static guint
mapvue_read_group52(const guchar **p, gsize size, gpointer grpdata,
                    GError **error)
{
    enum { SIZE = 16 };
    guint len, tagsize = 0;
    MapVueGroup52 *group = (MapVueGroup52*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field5, error)))
        return 0;
    tagsize += len;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size - tagsize))
        return 0;

    memcpy(group->time, *p, 8);
    *p += 8;
    memcpy(group->date, *p, 8);
    *p += 8;

    return tagsize + SIZE;
}

static guint
mapvue_read_group53(const guchar **p, gsize size, gpointer grpdata,
                    GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup53 *group = (MapVueGroup53*)grpdata;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field6, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field7, error)))
        return 0;
    tagsize += len;

    return tagsize;
}

static guint
mapvue_read_group101(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 16 };
    MapVueGroup101 *group = (MapVueGroup101*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->wedge = gwy_get_gfloat_le(p);
    group->testwedge = gwy_get_gfloat_le(p);
    group->wavelength = gwy_get_gfloat_le(p);
    group->new_wavelength = gwy_get_gfloat_le(p);
    return SIZE;
}

static guint
mapvue_read_group201(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 4 };
    MapVueGroup201 *group = (MapVueGroup201*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->data_scale_factor = gwy_get_gfloat_le(p);
    return SIZE;
}

static guint
mapvue_read_group451(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2 };
    MapVueGroup451 *group = (MapVueGroup451*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->n_segments = gwy_get_gint16_le(p);
    return SIZE;
}

static guint
mapvue_read_group501(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2 };
    MapVueGroup501 *group = (MapVueGroup501*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->data_type = gwy_get_gint16_le(p);
    return SIZE;
}

static guint
mapvue_read_group551(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 12 };
    MapVueGroup551 *group = (MapVueGroup551*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->magnification = gwy_get_gfloat_le(p);
    group->x_frame_scale = gwy_get_gfloat_le(p);
    group->y_optical_scale = gwy_get_gfloat_le(p);
    return SIZE;
}

static guint
mapvue_read_group651(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 8 };
    MapVueGroup651 *group = (MapVueGroup651*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->maximum_frames = gwy_get_guint16_le(p);
    group->display_rows = gwy_get_guint16_le(p);
    group->total_rows = gwy_get_guint16_le(p);
    group->total_columns = gwy_get_guint16_le(p);
    return SIZE;
}

static guint
mapvue_read_group801(const guchar **p, gsize size, gpointer grpdata,
                     GError **error)
{
    MapVueGroup801 *group = (MapVueGroup801*)grpdata;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(p, size, group->reftag, error)))
        return 0;

    group->system_information = gwy_get_guint16_le(p);

    return size;
}

static guint
mapvue_read_group852(const guchar **p, gsize size, gpointer grpdata,
                    GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup852 *group = (MapVueGroup852*)grpdata;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(p, size, group->reftag, error)))
        return 0;

    g_printerr("%u\n", (guint)size);
    if (!(len = mapvue_read_string(p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field5, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(p, size - tagsize, group->field6, error)))
        return 0;
    tagsize += len;

    return tagsize;
}

static guint
mapvue_read_group901(const guchar **p, gsize size, gpointer grpdata,
                    GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup901 *group = (MapVueGroup901*)grpdata;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(p, size - tagsize, group->comment, error)))
        return 0;
    tagsize += len;

    return tagsize;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
