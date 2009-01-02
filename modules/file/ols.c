/*
 *  @(#) $Id: psia.c 8034 2007-05-13 18:04:39Z yeti-dn $
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
 * <mime-type type="application/x-olympus-lex-3000">
 *   <comment>Olympus LEX 3000</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="II\x2a\x00"/>
 *   </magic>
 *   <glob pattern="*.ols"/>
 *   <glob pattern="*.OLS"/>
 * </mime-type>
 **/

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include "err.h"
#include <tiffio.h>

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

static gboolean      module_register      (void);
static gint          ols_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* ols_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyContainer* ols_load_tiff        (TIFF *tiff,
                                            GError **error);
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports OLS data files."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("ols",
                           N_("Olympus LEXT OLS3000 (.ols)"),
                           (GwyFileDetectFunc)&ols_detect,
                           (GwyFileLoadFunc)&ols_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gchar*
ols_read_value(const gchar* comment, const gchar* value_name)
{
   gchar *pos;

   pos = g_strstr_len(comment, strlen(comment), value_name);
   if (pos) {
      pos = g_strstr_len(pos, strlen(pos), "=");
      pos++;
   }
   if (!pos) {
      g_warning("Cannot find '%s' in file comment.\n", value_name);
   }
   return pos;
}

static gint
ols_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    TIFF *tiff = 0;
    gint score = 0;
    gchar *comment = 0;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = TIFFOpen(fileinfo->name, "r"))
        && TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &comment)
        && strstr(comment, "OLS"))
    {
        score = 100;
    }

    if (tiff)
        TIFFClose(tiff);

    return score;
}

static GwyContainer*
ols_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    TIFF *tiff;
    GwyContainer *container = NULL;

    tiff = TIFFOpen(filename, "r");
    if (!tiff)
        return NULL;

    container = ols_load_tiff(tiff, error);
    TIFFClose(tiff);

    return container;
}

static GwyContainer*
ols_load_tiff(TIFF *tiff, GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gint i, j, power10;
    gchar *comment = NULL, *data_channel_info_title, *data_channel_info;
    uint32 width, height;
    guchar *buffer;
    gchar channel_name[50];
    int dir_num = 0;
    double z_axis, xy_axis, factor;

    if (!TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &comment)
        || !strstr(comment, "OLS")) {
        err_FILE_TYPE(error, "OLS");
        return NULL;
    }

    z_axis = - atof(ols_read_value(comment, "ZPositionUp"))
             + atof(ols_read_value(comment, "ZPositionLow"));

    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    do {
        sprintf(channel_name, "/%d/data", dir_num++);
        data_channel_info_title = g_strdup_printf("[Data %d Info]", dir_num);
        // find channel info
        data_channel_info = g_strstr_len(comment, strlen(comment),
                                         data_channel_info_title);
        if (!data_channel_info) {
            g_warning("Cannot find '%s' in comment.", data_channel_info_title);
            continue;
        }
        if (!ols_read_value(data_channel_info, "XY Convert Value")) {
            g_warning("Cannot find 'XY Convert Value' in comment.");
            continue;
        }
        xy_axis = atof(ols_read_value(data_channel_info, "XY Convert Value"));

        buffer = g_new(guchar, TIFFScanlineSize(tiff));
        siunit = gwy_si_unit_new_parse("nm", &power10);
        dfield = gwy_data_field_new(width, height,
                                    width * xy_axis * pow10(power10),
                                    height * xy_axis * pow10(power10),
                                    FALSE);
        // units
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        siunit = gwy_si_unit_new_parse("um", &power10);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        factor = z_axis * pow10(power10)/4095.0;
        for (i = 0; i < height; i++) {
            gdouble *d;
            const guint16 *tiff_data = (const guint16*)buffer;
            TIFFReadScanline(tiff, buffer, i, 0);

            d = gwy_data_field_get_data(dfield) + (height-1 - i) * width;
            for (j = 0; j < width; j++)
                d[j] = tiff_data[j] * factor;
        }

        // add readed datafield to container
        if (!container)
            container = gwy_container_new();

        gwy_container_set_object_by_name(container, channel_name, dfield);

        // free resources
        g_object_unref(dfield);
        g_free(buffer);
        g_free(data_channel_info_title);
    } while (TIFFReadDirectory(tiff));

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
