/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti), Petr Klapetek, Daniil Bratashov (dn2010)
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com
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
 * <mime-type type="application/x-wipfile-spm">
 *   <comment>WITec Project data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="WIT_PRCT"/>
 *   </magic>
 *   <glob pattern="*.wip"/>
 *   <glob pattern="*.WIP"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * WIPfile
 * .wip
 * Read
 **/

/* FIXME: remove this in final version */
#define DEBUG
#include <stdio.h>
/* end */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "WIT_PRCT"
#define MAGIC_SIZE (8)

#define EXTENSION ".wip"

typedef enum {
    WIP_TAG_LIST     = 0, /* list of other tags */
    WIP_TAG_EXTENDED = 1, /* x86 FPU native type, 10 bytes */
    WIP_TAG_DOUBLE   = 2,
    WIP_TAG_FLOAT    = 3,
    WIP_TAG_INT64    = 4,
    WIP_TAG_INT32    = 5,
    WIP_TAG_UINT32   = 6,
    WIP_TAG_CHAR     = 7,
    WIP_TAG_BOOL     = 8, /* 1 byte */
    WIP_TAG_STRING   = 9  /* int32 = nchars, n bytes = string */
} WIPTagType;

gsize WIPTagDataSize[10] = {0, 10, 8, 4, 8, 4, 4, 1, 1, 0};

typedef enum {
    WIP_DATA_LIST     = 0, /* list of tags */
    WIP_DATA_INT64    = 1,
    WIP_DATA_INT32    = 2,
    WIP_DATA_INT16    = 3,
    WIP_DATA_INT8     = 4,
    WIP_DATA_UINT32   = 5,
    WIP_DATA_UINT16   = 6,
    WIP_DATA_UINT8    = 7,
    WIP_DATA_BOOL     = 8, /* 1 byte */
    WIP_DATA_FLOAT    = 9,
    WIP_DATA_DOUBLE   = 10,
    WIP_DATA_EXTENDED = 11 /* x86 FPU native type, 10 bytes */
} WIPDataType;

typedef struct {
    guint32       name_length;
    const guchar *name; /* name_length bytes */
    WIPTagType    type;
    gint64        data_start;
    gint64        data_end;
    const guchar *data;
} WIPTag;

/*
typedef struct {
    gint32 version;
} WIPFile;
*/

static gboolean       module_register       (void);
static gint           wip_detect            (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static WIPTag*        wip_get_tag           (guchar **pos, gsize *size);
static void           wip_free_tag          (WIPTag *tag);
static GwyContainer*  wip_load              (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
/*
static gboolean       wip_real_load         (const guchar *buffer,
                                             gsize size,
                                             WIPFile *wipfile,
                                             GError **error);
*/

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports WItec Project data files."),
    "dn2010 <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("wipfile",
                           N_("WItec Project files (.wip)"),
                           (GwyFileDetectFunc)&wip_detect,
                           (GwyFileLoadFunc)&wip_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
wip_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static void print_tag_data(WIPTag *tag, guchar **pos, guint asterisks)
{
    guchar *p;
    gchar *str;
    gint i, j, n, str_len;

    if (WIPTagDataSize[tag->type])
        n = (tag->data_end - tag->data_start)/WIPTagDataSize[tag->type];
    else if (tag->type) // string
        n = 1;
    else // container
        n = 0;

    if (n > 10) {
        for (j = 0; j < asterisks; j++)
            fprintf(stderr,"  ");
        fprintf(stderr,"%d 0x%X\n", n, tag->data_start);
        n = 0; // printing offset instead of data
    }

    p = *pos;
    for (i = 0; i < n; i++) {
        for (j = 0; j < asterisks; j++)
            fprintf(stderr,"  ");
        switch(tag->type) {
            case 0:
                break;
            case 1:
                fprintf(stderr,"pascal extended ");
                break;
            case 2:
                fprintf(stderr,"%g ",gwy_get_gdouble_le(&p));
                break;
            case 3:
                fprintf(stderr,"%g ",gwy_get_gfloat_le(&p));
                break;
            case 4:
                fprintf(stderr,"%d ",gwy_get_gint64_le(&p));
                break;
            case 5:
                fprintf(stderr,"%d ",gwy_get_gint32_le(&p));
                break;
            case 6:
                fprintf(stderr,"%u ",gwy_get_guint32_le(&p));
                break;
            case 7:
                fprintf(stderr,"%u ",(*p++));
                break;
            case 8:
                fprintf(stderr,"%u ",(*p++));
                break;
            case 9:
                str_len = gwy_get_gint32_le(&p);
                str = g_strndup(p, str_len);
                fprintf(stderr,"%s",str);
                g_free(str);
                break;
            default:
                fprintf(stderr,"something wrong ");
        }
        fprintf(stderr,"\n");
    }
}

static WIPTag *wip_get_tag(guchar **pos, gsize *size)
{
    WIPTag *tag;
    guchar *p;

    p = *pos;
    if (*size < 4)
        return NULL;
    tag = g_new0(WIPTag, 1);
    tag->name_length = gwy_get_guint32_le(&p);
    if (*size < 24+tag->name_length) {
        g_free(tag);
        return NULL;
    }
    tag->name = g_strndup(p, tag->name_length);
    p += tag->name_length;
    tag->type = (WIPTagType)gwy_get_guint32_le(&p);
    tag->data_start = gwy_get_gint64_le(&p);
    tag->data_end = gwy_get_gint64_le(&p);
    /*
    fprintf(stderr,"%d %s %d %lld %lld\n",  tag->name_length,
                tag->name, tag->type, tag->data_start, tag->data_end);
    */
    *pos = p;
    *size -= 24+tag->name_length;
    return tag;
}

static void wip_free_tag(WIPTag *tag)
{
    g_free(tag->name);
    g_free(tag);
}

static void print_tags (const guchar *buffer, gsize pos,
                        gsize end, gint n)
{
    guchar *p;
    gsize cur;
    gsize remaining;
    WIPTag *tag;
    gint i;

    cur = pos;
    p = (guchar *)(buffer + pos);
    while(cur < end) {
        p = (guchar *)(buffer + cur);
        remaining = end - cur;
        if(!(tag = wip_get_tag(&p,&remaining))) {
        }
        else {
            for (i = 0; i < n; i++)
                fprintf(stderr,"* ");
            fprintf(stderr,"%s \n", tag->name);
            if(!tag->type)
                print_tags(buffer, tag->data_start, tag->data_end, n+1);
            else {
                p = (guchar *)(buffer + tag->data_start);
                print_tag_data(tag, &p, n+1);
            }
            cur = tag->data_end;
            wip_free_tag(tag);
        }
    }
}

static GwyContainer*
wip_load(const gchar *filename, GwyRunType mode, GError **error)
{
    guchar *buffer;
    gsize size, remaining;
    GError *err = NULL;
    guchar *p;
    WIPTag *tag;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer + 8; /* skip magic header */

    print_tags(buffer,8, size, 0);

    gwy_file_abandon_contents(buffer, size, NULL);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
