/*
 *  @(#) $Id: quesant.c 8632 2007-10-11 07:59:01Z yeti-dn $
 *  Copyright (C) 2004-2007 David Necas (Yeti), Petr Klapetek.
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
 * <mime-type type="application/x-quesant-afm">
 *   <comment>Quesant AFM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\\*File list\r\n"/>
 *     <match type="string" offset="0" value="?*File list\r\n"/>
 *   </magic>
 * </mime-type>
 **/
// TODO: not sure about picture orientation

#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"


#define MAGIC "\x02\x00\x00\x00\x01\x00\x00\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define HEAD_LENGTH 0x147

typedef char bool;

static gboolean        module_register       (void);
static gint            quesant_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   quesant_load         (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Quesant file format"
       "."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("quesant",
                           N_("quesant files"),
                           (GwyFileDetectFunc)&quesant_detect,
                           (GwyFileLoadFunc)&quesant_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
quesant_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static guint32 
quesant_find_gint16(gchar *haystack, gchar *needle)
{
   gint32 res = 0;
   gchar* found_str = strstr(haystack, needle);
   if (found_str) {
      memcpy(&res, found_str+(sizeof(needle)), sizeof(gint32));
   }
   return res;
 
}

typedef struct {
   guint32 desc_loc;       // offset of description (unused)
   guint32 date_loc;       // date location (unused)
   guint32 plet_loc;       // unknown 
   guint32 image_loc;      // offset of image data (first int16 
                           // is number of rows and cols)
   guint32 img_p_loc;      // offset of Z axis multiply
   guint32 hard_loc;       // offset of X/Y axis width/height
   guint32 short_desc_loc; // offset of short desc (unused)
   guint16 img_size;       // size of image

} FileInfo;

static void
read_file_info(gchar *buffer, gsize size, FileInfo *info)
{
   int pos = MAGIC_SIZE;

   // clear info structure
   memset(info, 0, sizeof(FileInfo));
   // read structure variables from buffer
   while (pos < HEAD_LENGTH) {
      while (buffer[pos] == 0 && pos < HEAD_LENGTH)
         pos++;
      if (info->desc_loc == 0)
         info->desc_loc = quesant_find_gint16(buffer + pos, "DESC");
      if (info->date_loc == 0)
         info->date_loc = quesant_find_gint16(buffer + pos, "DATE");
      if (info->plet_loc == 0)
         info->plet_loc = quesant_find_gint16(buffer + pos, "PLET");
      if (info->image_loc == 0)
         info->image_loc = quesant_find_gint16(buffer + pos, "IMAG");
      if (info->hard_loc == 0) 
         info->hard_loc = quesant_find_gint16(buffer + pos, "HARD");
      if (info->img_p_loc == 0)
         info->img_p_loc = quesant_find_gint16(buffer + pos, "IMGP");
      if (info->short_desc_loc == 0)
         info->short_desc_loc = quesant_find_gint16(buffer + pos, "SDES");
      pos += 8;
   }
   // check if read locations values are in buffer
   if (info->image_loc > size 
         || info->img_p_loc > size 
         || info->hard_loc > size
         || info->plet_loc > size)
   {
      memset(info, 0, sizeof(FileInfo));
   } else {
      info->img_size = *(guint16*)(buffer + info->image_loc);
   }

}


static GwyContainer*
quesant_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
   GwyContainer *container = NULL;
   gchar *buffer = NULL;
   gsize size = 0;
   GError *err = NULL;
   GwySIUnit *units = NULL;
   gint32 power10;
   FileInfo info;
   int row, col;
   GwyDataField *dfield;
   guint16 *val;
   float multiplier;

   if (!g_file_get_contents(filename, &buffer, &size, &err)) {
      err_GET_FILE_CONTENTS(error, &err);
      return NULL;
   }

   read_file_info(buffer, size, &info);

   if ( info.image_loc == 0 
         || info.img_p_loc == 0 
         || info.img_size == 0 ) 
   {
      return NULL;
   }
   // create units and datafield
   units = gwy_si_unit_new_parse("um", &power10);

   dfield = gwy_data_field_new(info.img_size, info.img_size, 
         *(float*)((buffer + info.hard_loc)) * pow10(power10), 
         *(float*)((buffer + info.hard_loc)) * pow10(power10), 
         TRUE);
   // set units for XY axes
   gwy_data_field_set_si_unit_xy(dfield, units);
   g_object_unref(units);
   // set units for Z axis
   units = gwy_si_unit_new_parse("um", &power10);
   gwy_data_field_set_si_unit_z(dfield, units);
   g_object_unref(units);

   multiplier = *((float*) (buffer + info.img_p_loc + 8)) * pow10(power10);
   // values are stored in unsigned int16 type
   val = (guint16*) (buffer + info.image_loc);
   for (row = 0; row < info.img_size; row++)
      for (col = info.img_size-1; col >= 0; col--) {
         gwy_data_field_set_val(dfield, row, col, (*val) * multiplier);
         val++;
      }
   // create container
   container = gwy_container_new();
   // put datafield into container
   gwy_container_set_object_by_name(container, "/0/data", dfield);
   g_object_unref(dfield);
   return container;
}



