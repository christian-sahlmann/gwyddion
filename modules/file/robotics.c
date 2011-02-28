/*
 *  @(#) $Id: shimadzu.c 11275 2010-05-03 07:29:30Z yeti-dn $
 *  Copyright (C) 2007 David Necas (Yeti).
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
 * <mime-type type="application/x-robotics-spm">
 *   <comment>Automation and Robotics Dual Lensmapper data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Shimadzu SPM File Format"/>
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
 * Shimadzu
 * <!--FIXME-->
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
    guint version, origin, i, xres, yres, valid;
    gchar **splitted_line = NULL;
    GwyDataField *dfield_dpt = NULL, *dfield_sph = NULL, *dfield_cyl = NULL, *dfield_axis = NULL;
    GwyDataField *dfield_normx = NULL, *dfield_normy = NULL, *dfield_normz = NULL, *dfield_posz = NULL;
    GwyDataField *dfield_mincurvx = NULL, *dfield_mincurvy = NULL, *dfield_mincurvz = NULL;
    gdouble xreal, yreal, xoffset, yoffset, dpt, sph, cyl, axis, normx, normy, normz, posz, mincurvx, mincurvy, mincurvz;
    gdouble *data_dpt, *data_sph, *data_cyl, *data_axis, *data_normx, *data_normy, *data_normz, *data_posz;
    gdouble *data_mincurvx, *data_mincurvy, *data_mincurvz;

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
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }

    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "Carto origin (0=Refl, 1=Transm, 2=Extern):\t%u",&origin) != 1) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }
    
    for (i = 1; i < 11; i++)
         line = gwy_str_next_line(&p);
    
    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "Nbs Points (x,y):\t%u\t%u",&xres, &yres) != 2) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }
    
    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "Size (x,y in mm):\t%lf\t%lf",&xreal, &yreal) != 2) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }

    line = gwy_str_next_line(&p);

    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%u\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf",
                  &xoffset, &yoffset, &dpt, &sph, &cyl, &axis, &valid, &normx, &normy, &normz,
                  &posz, &mincurvx, &mincurvy, &mincurvz) != 14) {
        err_FILE_TYPE(error, "Automation & Robotics");
        goto fail;
    }

    dfield_dpt = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_dpt = gwy_data_field_get_data(dfield_dpt);
    dfield_sph = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_sph = gwy_data_field_get_data(dfield_sph);
    dfield_cyl = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_cyl = gwy_data_field_get_data(dfield_cyl);
    dfield_axis = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_axis = gwy_data_field_get_data(dfield_axis);
    dfield_normx = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_normx = gwy_data_field_get_data(dfield_normx);
    dfield_normy = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_normy = gwy_data_field_get_data(dfield_normy);
    dfield_normz = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_normz = gwy_data_field_get_data(dfield_normz);
    dfield_posz = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_posz = gwy_data_field_get_data(dfield_posz);
    dfield_mincurvx = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_mincurvx = gwy_data_field_get_data(dfield_mincurvx);
    dfield_mincurvy = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_mincurvy = gwy_data_field_get_data(dfield_mincurvy);
    dfield_mincurvz = gwy_data_field_new(xres, yres, xreal*0.001, yreal*0.001, TRUE);
    data_mincurvz = gwy_data_field_get_data(dfield_mincurvz);

    if (valid) {
        data_dpt[0] = dpt;
        data_sph[0] = sph;
        data_cyl[0] = cyl;
        data_axis[0] = axis;
        data_normx[0] = normx;
        data_normy[0] = normy;
        data_normz[0] = normz;
        data_posz[0] = posz;
        data_mincurvx[0] = mincurvx;
        data_mincurvy[0] = mincurvy;
        data_mincurvz[0] = mincurvz;
    }
    
    gwy_data_field_set_xoffset(dfield_dpt, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_dpt, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_sph, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_sph, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_cyl, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_cyl, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_axis, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_axis, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_normx, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_normx, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_normy, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_normy, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_normz, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_normz, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_posz, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_posz, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_mincurvx, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_mincurvx, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_mincurvy, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_mincurvy, yoffset*0.001);
    gwy_data_field_set_xoffset(dfield_mincurvz, xoffset*0.001);
    gwy_data_field_set_yoffset(dfield_mincurvz, yoffset*0.001);
    
    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield_dpt, unit);
    gwy_data_field_set_si_unit_xy(dfield_sph, unit);
    gwy_data_field_set_si_unit_xy(dfield_cyl, unit);
    gwy_data_field_set_si_unit_xy(dfield_axis, unit);
    gwy_data_field_set_si_unit_xy(dfield_normx, unit);
    gwy_data_field_set_si_unit_xy(dfield_normy, unit);
    gwy_data_field_set_si_unit_xy(dfield_normz, unit);
    gwy_data_field_set_si_unit_xy(dfield_posz, unit);
    gwy_data_field_set_si_unit_xy(dfield_mincurvx, unit);
    gwy_data_field_set_si_unit_xy(dfield_mincurvy, unit);
    gwy_data_field_set_si_unit_xy(dfield_mincurvz, unit);
    g_object_unref(unit);
         
    line = gwy_str_next_line(&p);
    for (i = 1; i < xres*yres; i++) {
         if (!line
             || sscanf(line, "%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%u\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf",
                       &xoffset, &yoffset, &dpt, &sph, &cyl, &axis, &valid, &normx, &normy, &normz,
                       &posz, &mincurvx, &mincurvy, &mincurvz) != 14) {
             err_FILE_TYPE(error, "Automation & Robotics");
             goto fail;
         }
         if (valid) {
             data_dpt[i] = dpt;
             data_sph[i] = sph;
             data_cyl[i] = cyl;
             data_axis[i] = axis;
             data_normx[i] = normx;
             data_normy[i] = normy;
             data_normz[i] = normz;
             data_posz[i] = posz;
             data_mincurvx[i] = mincurvx;
             data_mincurvy[i] = mincurvy;
             data_mincurvz[i] = mincurvz;
         }
         line = gwy_str_next_line(&p);
    }
      
    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield_dpt);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(1), dfield_sph);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(2), dfield_cyl);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(3), dfield_axis);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(4), dfield_normx);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(5), dfield_normy);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(6), dfield_normz);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(7), dfield_posz);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(8), dfield_mincurvx);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(9), dfield_mincurvy);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(10), dfield_mincurvz);

    gwy_container_set_string_by_name(container, "/0/data/title", "Dpt");
    gwy_container_set_string_by_name(container, "/1/data/title", "Sph");
    gwy_container_set_string_by_name(container, "/2/data/title", "Cyl");
    gwy_container_set_string_by_name(container, "/3/data/title", "Axis");
    gwy_container_set_string_by_name(container, "/4/data/title", "NormX");
    gwy_container_set_string_by_name(container, "/5/data/title", "NormY");
    gwy_container_set_string_by_name(container, "/6/data/title", "NormZ");
    gwy_container_set_string_by_name(container, "/7/data/title", "PosZ");
    gwy_container_set_string_by_name(container, "/8/data/title", "MinCurvX");
    gwy_container_set_string_by_name(container, "/9/data/title", "MinCurvY");
    gwy_container_set_string_by_name(container, "/10/data/title", "MinCurvZ");
    
    meta = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    gwy_container_set_object_by_name(container, "/1/meta", meta);
    gwy_container_set_object_by_name(container, "/2/meta", meta);
    gwy_container_set_object_by_name(container, "/3/meta", meta);
    gwy_container_set_object_by_name(container, "/4/meta", meta);
    gwy_container_set_object_by_name(container, "/5/meta", meta);
    gwy_container_set_object_by_name(container, "/6/meta", meta);
    gwy_container_set_object_by_name(container, "/7/meta", meta);
    gwy_container_set_object_by_name(container, "/8/meta", meta);
    gwy_container_set_object_by_name(container, "/9/meta", meta);
    gwy_container_set_object_by_name(container, "/10/meta", meta);
    g_object_unref(meta);

    //gwy_container_set_string_by_name(meta, "File version:", g_strdup_printf("%d", version));
    gwy_container_set_string_by_name(meta, "Comment:", g_strdup_printf("%s", comment));
    
    switch (origin) {
            case 0:
            gwy_container_set_string_by_name(meta, "Carto origin:", "Refl");
            break;
              
            case 1:
            gwy_container_set_string_by_name(meta, "Carto origin:", "Transm");
            break;
              
            case 2:
            gwy_container_set_string_by_name(meta, "Carto origin:", "Extern");
            break;
            
            default:
            g_assert_not_reached();
            break;
    }
    
    gwy_container_set_string_by_name(meta, "Nbs Points (x,y):", g_strdup_printf("%u,%u", xres, yres));
    gwy_container_set_string_by_name(meta, "Size (x,y in mm):", g_strdup_printf("%.3lf,%.3lf", xreal, yreal));

fail:
    g_free(buffer);
    g_strfreev(splitted_line);
    
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
