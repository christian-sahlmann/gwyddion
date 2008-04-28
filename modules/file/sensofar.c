/*
 *  @(#) $Id: sensofar.c 8632 2007-10-11 07:59:01Z yeti-dn $
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
 * <mime-type type="application/x-sensofar-plu">
 *   <comment>Sensofar PLu data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\\*File list\r\n"/>
 *     <match type="string" offset="0" value="?*File list\r\n"/>
 *   </magic>
 * </mime-type>
 **/

#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"


#define MAX_LONG_DATA 128
#define MAX_LONG_COMENTARI 256

typedef char bool;

typedef enum {
   MES_IMATGE, MES_PERFIL, MES_MULTIPERFIL, MES_TOPO, 
   MES_COORDENADES, MES_GRUIX, MES_CUSTOM
} measure_types;

typedef struct {
   char S[MAX_LONG_DATA];
   time_t t;
} tData;

typedef struct {
   gint32 tipus;
   gint32 algor;
   gint32 metode;
   gint32 Obj;
   gint32 Area;
   gint32 N_area;
   gint32 M_area;
   gint32 N;
   gint32 M;
   gint32 na;
   double incr_z;
   float rang;
   gint32 m_plans;
   gint32 tpc_umbral_F;
   bool restore;
   unsigned char num_layers;
   unsigned char version;
   unsigned char config_hardware;
   unsigned char stack_im_num;
   unsigned char reserve;
   gint32 factor_delmacio;
} tConfigMesura;

typedef struct  {
   gint32 N;
   gint32 M;
   gint32 N_tall;
   float dy_multip;
   float mppx;
   float mppy;
   float x_0;
   float y_0;
   float mpp_tall;
   float z_0;
} tCalibratEixos_Arxiu;

typedef struct {
   tData date;
   char user_comment[MAX_LONG_COMENTARI];
   tCalibratEixos_Arxiu axes_config;
   tConfigMesura measure_config;
} tDataDesc;

static gboolean        module_register       (void);
static gint            sensofar_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   sensofar_load         (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Sensofar PLu file format, "
       "version 2000 or newer."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sensofar",
                           N_("Sensofar PLu files"),
                           (GwyFileDetectFunc)&sensofar_detect,
                           (GwyFileLoadFunc)&sensofar_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
sensofar_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;
    char day_name[4], month_name[4];
    int month_day, hour, min, sec, year;

    if (only_name)
        return 0;
    // File starts with date, try to parse it.
    // FIXME: this is stupid
    if (fileinfo->buffer_len > sizeof(tData)) {
       if (sscanf(fileinfo->head, "%3s %3s %u %u:%u:%u %u", 
             day_name, 
             month_name, 
             &month_day, 
             &hour, 
             &min, 
             &sec, 
             &year) == 7) 
       {
          score = 100;
       }

    }

    return score;
}

static GwyContainer*
sensofar_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
   tDataDesc *data_desc;
   GwyContainer *container = NULL;
   gchar *buffer = NULL;
   gsize size = 0;
   GError *err = NULL;
   gint32 rows = 0, cols = 0, i, j;
   float *data_start;
   GwyDataField *d;
   GwySIUnit *units = NULL;
   gint32 power10;

   if (!g_file_get_contents(filename, &buffer, &size, &err)) {
      err_GET_FILE_CONTENTS(error, &err);
      return NULL;
   }
   if (size < sizeof(tDataDesc) + 12) {
      g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                  _("File too small (less than %d bytes). Header is not complete."), 
                  sizeof(tDataDesc)+12);
      return NULL;
   }
   // A little dirty, set pointer to data description structure to start of buffer
   // to avoid copying, reading from buffer and writing to data structure.
   data_desc = (tDataDesc*) buffer;
   gwy_debug("\nFile date: %s\nData type: %d \nData width: %d\nData height: %d\nVersion: %d", 
         data_desc->date.S, 
         data_desc->measure_config.tipus, 
         data_desc->measure_config.N, 
         data_desc->measure_config.M,
         data_desc->measure_config.version);


   switch (data_desc->measure_config.tipus) 
   {
      case MES_TOPO:
      case MES_IMATGE:
      case MES_MULTIPERFIL:
         memcpy(&cols, buffer + sizeof(tDataDesc), 4);
         memcpy(&rows, buffer + sizeof(tDataDesc)+4, 4);
         gwy_debug("Data size: %dx%d", rows, cols);
         data_start = (float *) (buffer + sizeof(tDataDesc)+8);

         units = gwy_si_unit_new_parse("um", &power10); // values are in um only
         d = gwy_data_field_new(
               rows, 
               cols, 
               data_desc->axes_config.mppy * rows * pow10(power10), 
               data_desc->axes_config.mppx * cols * pow10(power10), 
               TRUE);
         gwy_data_field_set_si_unit_xy(d, units);
         gwy_data_field_set_si_unit_z(d, units);
         for (i = 0; i < cols; i++) {
            for (j = 0; j < rows; j++) {
               // FIXME: lost pixel, such value makes problem to gwyddion,
               // setting to 0.0f, undefined value would be better though.
               if (*data_start == 1000001.0) 
                  *data_start = 0.0f;
               gwy_data_field_set_val(d, j, i, *data_start*pow10(power10));
               data_start++;
            }
         }
         gwy_debug("Offset: %f %f", data_desc->axes_config.x_0, 
               data_desc->axes_config.y_0);
         //FIXME: offset later, support of offset determined by version?
         //gwy_data_field_set_xoffset(d, pow10(power10)*data_desc->axes_config.x_0);
         //gwy_data_field_set_yoffset(d, pow10(power10)*data_desc->axes_config.y_0);
         
         container = gwy_container_new();
         gwy_container_set_object_by_name(container, "/0/data", d);
         break;
      default:
         g_warning("Data type '%d' not implemented.", 
               data_desc->measure_config.tipus);
         g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                     _("Data type '%d' is not implemented."), 
                     data_desc->measure_config.tipus);
         
         break;
   }
   return container;
}



