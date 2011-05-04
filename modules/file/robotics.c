/*
 *  @(#) $Id: shimadzu.c 11275 2010-05-03 07:29:30Z yeti-dn $
 *  Copyright (C) 2011 Miroslav Valtr (Mira).
 *  E-mail: miraval@seznam.cz.
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
 * <mime-type type="application/x-robotics-spm">
 *   <comment>Automation and Robotics Dual Lens Mapper data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="File version:"/>
 *   </magic>
 *   <glob pattern="*.mcr"/>
 *   <glob pattern="*.MCR"/>
 *   <glob pattern="*.mct"/>
 *   <glob pattern="*.MCT"/>
 *   <glob pattern="*.mce"/>
 *   <glob pattern="*.MCE"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Automation and Robotics Dual Lens Mapper
 * .mcr .mct .mce
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC "File version:\t0"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION_MCR ".mcr"
#define EXTENSION_MCT ".mct"
#define EXTENSION_MCE ".mce"
#define NUM_DFIELDS 14

static gboolean      module_register      (void);
static gint          robotics_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* robotics_load        (const gchar *filename,
                                           G_GNUC_UNUSED GwyRunType mode,
                                           GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Automation & Robotics Dual Lensmapper data files"),
    "Mira <miraval@seznam.cz>",
    "0.1",
    "Miroslav Valtr",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("robotics",
                           N_("Dual Lensmapper files"),
                           (GwyFileDetectFunc)&robotics_detect,
                           (GwyFileLoadFunc)&robotics_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
robotics_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name) {
      if (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_MCR) ||
          g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_MCT) ||
          g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_MCE))
        return 10;
      else
        return 0;
    }

    if (fileinfo->file_size < MAGIC_SIZE + 2
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 50;
}

static GwyContainer*
robotics_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL, *meta;
    GError *err = NULL;
    gsize size = 0;
    gchar *line, *p, *buffer = NULL, *comment = NULL;
    GwySIUnit *unit = NULL;
    guint version, origin, i, j, xres, yres;
    gint valid;
    gchar **splitted_line = NULL;
    GwyDataField **dfield = NULL;
    gdouble xreal, yreal, xoffset, yoffset;
    gdouble **data = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "File version:\t%u",&version) != 1) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }

    line = gwy_str_next_line(&p);
    splitted_line = g_strsplit(line,"\t",2);
    comment = splitted_line[1];
    if (!line || comment == NULL) {
        err_MISSING_FIELD(error, "Comment");
        goto fail;
    }

    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "Carto origin (0=Refl, 1=Transm, 2=Extern):\t%u",&origin) != 1) {
        err_MISSING_FIELD(error, "Origin");
        goto fail;
    }
    
    for (i = 1; i < 11; i++)
         line = gwy_str_next_line(&p);
    
    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "Nbs Points (x,y):\t%u\t%u",&xres, &yres) != 2) {
        err_MISSING_FIELD(error, "Nbs Points (x,y)");
        goto fail;
    }
    
    line = gwy_str_next_line(&p);
    splitted_line = g_strsplit(line,"\t",3);
    if (!line || splitted_line == NULL) {
        err_MISSING_FIELD(error, "Size (x,y in mm)");
        goto fail;
    }
    //0.001 is the conversion factor from mm to m
    xreal = 0.001*g_ascii_strtod(splitted_line[1], NULL);
    yreal = 0.001*g_ascii_strtod(splitted_line[2], NULL);
    
    line = gwy_str_next_line(&p);

    line = gwy_str_next_line(&p);
    splitted_line = g_strsplit(line,"\t",NUM_DFIELDS+1);
    if (!line || splitted_line == NULL) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }
    //0.001 is the conversion factor from mm to m
    xoffset = 0.001*g_ascii_strtod(splitted_line[0], NULL);
    yoffset = 0.001*g_ascii_strtod(splitted_line[1], NULL);
    valid = g_ascii_strtod(splitted_line[6], NULL);

    dfield = g_new(GwyDataField*, NUM_DFIELDS);
    data = g_new(gdouble*, NUM_DFIELDS);
    for (i=0; i<NUM_DFIELDS; i++) {
         dfield[i] = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);
         data[i] = gwy_data_field_get_data(dfield[i]);
         gwy_data_field_set_xoffset(dfield[i], xoffset);
         gwy_data_field_set_yoffset(dfield[i], yoffset);
         unit = gwy_si_unit_new("m");
         gwy_data_field_set_si_unit_xy(dfield[i], unit);
         g_object_unref(unit);
    }
    
    if (valid) {
      for (i=0; i<NUM_DFIELDS; i++) {
        data[i][0] = g_ascii_strtod(splitted_line[i], NULL);
      }
    }
    
    for (j=1; j<(xres*yres);j++) {
      line = gwy_str_next_line(&p);
      if (!line) {
          err_TOO_SHORT(error);
          goto fail;
      }
      splitted_line = g_strsplit(line,"\t",NUM_DFIELDS+1);
      if (splitted_line == NULL) {
          err_FILE_TYPE(error, "Automation & Robotics");
          goto fail;
      }
      valid = g_ascii_strtod(splitted_line[6], NULL);
      if (valid) {
        for (i=0; i<NUM_DFIELDS; i++)
          data[i][j] = g_ascii_strtod(splitted_line[i], NULL);
      }
    }
    
    container = gwy_container_new();
    for (i=0; i<NUM_DFIELDS; i++) {
      gwy_container_set_object(container, gwy_app_get_data_key_for_id(i), dfield[i]);
      g_object_unref(dfield[i]);
    }
    
    gwy_container_set_string_by_name(container, "/0/data/title", g_strdup("PosX"));
    gwy_container_set_string_by_name(container, "/1/data/title", g_strdup("PosY"));
    gwy_container_set_string_by_name(container, "/2/data/title", g_strdup("Dpt"));
    gwy_container_set_string_by_name(container, "/3/data/title", g_strdup("Sph"));
    gwy_container_set_string_by_name(container, "/4/data/title", g_strdup("Cyl"));
    gwy_container_set_string_by_name(container, "/5/data/title", g_strdup("Axis"));
    gwy_container_set_string_by_name(container, "/6/data/title", g_strdup("Valid"));
    gwy_container_set_string_by_name(container, "/7/data/title", g_strdup("NormX"));
    gwy_container_set_string_by_name(container, "/8/data/title", g_strdup("NormY"));
    gwy_container_set_string_by_name(container, "/9/data/title", g_strdup("NormZ"));
    gwy_container_set_string_by_name(container, "/10/data/title", g_strdup("PosZ"));
    gwy_container_set_string_by_name(container, "/11/data/title", g_strdup("MinCurvX"));
    gwy_container_set_string_by_name(container, "/12/data/title", g_strdup("MinCurvY"));
    gwy_container_set_string_by_name(container, "/13/data/title", g_strdup("MinCurvZ"));
    
    meta = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    //gwy_container_set_string_by_name(meta, "File version:", g_strdup_printf("%d", version));
    gwy_container_set_string_by_name(meta, "Comment:", g_strdup_printf("%s", comment));
    
    switch (origin) {
            case 0:
            gwy_container_set_string_by_name(meta, "Carto origin:", g_strdup("Refl"));
            break;
              
            case 1:
            gwy_container_set_string_by_name(meta, "Carto origin:", g_strdup("Transm"));
            break;
              
            case 2:
            gwy_container_set_string_by_name(meta, "Carto origin:", g_strdup("Extern"));
            break;
            
            default:
            g_assert_not_reached();
            break;
    }
    
    gwy_container_set_string_by_name(meta, "Nbs Points (x,y):", g_strdup_printf("%u,%u", xres, yres));
    gwy_container_set_string_by_name(meta, "Size (x,y in mm):", g_strdup_printf("%.3lf,%.3lf", xreal*1000.0, yreal*1000.0));

fail:
    g_free(buffer);
    if (*dfield != NULL)
      g_free(dfield);
    if (*data != NULL)
      g_free(data);
  
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
