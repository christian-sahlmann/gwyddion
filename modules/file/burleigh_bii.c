/*
 *  @(#) $Id: burleigh_bii.c 8625 2007-10-10 20:03:12Z yeti-dn $
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Hans-Peter Doerr.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *          doerr@cip.physik.uni-freiburg.de.
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
//TODO: filemagic
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-burleigh-bii-spm">
 *   <comment>Burleigh binary SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\xff\x06@\x00"/>
 *   </magic>
 *   <glob pattern="*.bii"/>
 *   <glob pattern="*.BII"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <libprocess/stats.h>

#include "err.h"

#define EXTENSION "bii"
#define FILE_START "BM6"
#define FILE_IMG_WIDTH_POS 0x12
#define FILE_IMG_HEIGHT_POS 0x16
#define FILE_IMG_DATA_START_POS 0x36
#define FILE_IMG_X_SCALE_POS 0x74
#define FILE_IMG_Y_SCALE_POS 0x7C
#define FILE_IMG_Z_SCALE_POS 0xCC

static gboolean      module_register  (void);
static gint          burleigh_bii_detect  (const GwyFileDetectInfo *fileinfo,
                                       gboolean only_name);
static GwyContainer* burleigh_bii_load    (const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh BII binary data files."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Hans-Peter Doerr",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("burleigh_bii",
                           N_("Burleigh Image Studio files (.bii)"),
                           (GwyFileDetectFunc)&burleigh_bii_detect,
                           (GwyFileLoadFunc)&burleigh_bii_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
burleigh_bii_detect(const GwyFileDetectInfo *fileinfo,
                    gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > 3 && strncmp(fileinfo->head, FILE_START, 3) == 0 ) {
        return 100;     
    }

    return 0;
}

static GwyContainer*
burleigh_bii_load(const gchar *filename,
                  G_GNUC_UNUSED GwyRunType mode,
                  GError **error)
{
   gchar *buffer = NULL;
   gint size = 0;
   GError *err = NULL;
   gint width, height, data_end_offset, i, j, power10;
   gint16 *values;
   gdouble x_scale, y_scale, z_scale;
   GwyDataField *d;
   GwyContainer *container;
   GwySIUnit *units = NULL;

   if (!g_file_get_contents(filename, &buffer, &size, &err)) {
      err_GET_FILE_CONTENTS(error, &err);
      return NULL;      
   }
   if (size > FILE_IMG_DATA_START_POS) {
      // get datafield dimensions from file
      memcpy(&width, buffer + FILE_IMG_WIDTH_POS, 4);
      memcpy(&height, buffer + FILE_IMG_HEIGHT_POS, 4);
      // get file offset where datafield ends
      data_end_offset = width * height * 2 + FILE_IMG_DATA_START_POS;      
      // get scaling factors of axes
      if (data_end_offset + FILE_IMG_Z_SCALE_POS + 8 > size) {
         g_warning("File is too small, expected size at least %d (size: %d)", 
               data_end_offset + FILE_IMG_Z_SCALE_POS + 8, size);
         return NULL;
      }
      memcpy(&x_scale, buffer + data_end_offset + FILE_IMG_X_SCALE_POS, 8);
      memcpy(&y_scale, buffer + data_end_offset + FILE_IMG_Y_SCALE_POS, 8);
      memcpy(&z_scale, buffer + data_end_offset + FILE_IMG_Z_SCALE_POS, 8);
      
      //FIXME: all axes always in nm?
      units = gwy_si_unit_new_parse("nm", &power10);
      gwy_debug("w:%d h:%d scale: x%f y%f z%f", 
            width, height, x_scale * pow10(power10), 
            y_scale, z_scale);
      // create new datafield
      d = gwy_data_field_new( 
            width, 
            height, 
            x_scale * pow10(power10), 
            y_scale * pow10(power10), 
            TRUE);
      gwy_data_field_set_si_unit_xy(d, units);
      gwy_data_field_set_si_unit_z(d, units);
      // set pointer to 16bit signed integers to start position 
      // of data definition in file
      values = (gint16 *)(buffer + FILE_IMG_DATA_START_POS);
      // fill datafield
      for (j = 0; j < height; j++)
         for (i = 0; i < width; i++) {
            gwy_data_field_set_val(
                  d, 
                  i, 
                  j, 
                  (gdouble)(*values * z_scale) * pow10(power10));
            values++;
         }
      // fix zero
      gwy_data_field_add(d, -gwy_data_field_get_min(d));
      // fill container
      container = gwy_container_new();
      gwy_container_set_object_by_name(container, "/0/data", d);
      return container;
   } else {
      g_warning("File is too small.");
   }
   return NULL;
}
