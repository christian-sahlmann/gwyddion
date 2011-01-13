/*
 *  @(#) $Id: gwycalibration.c 7021 2006-11-20 20:32:29Z yeti-dn $
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/gwycalibration.h>

static void         gwy_calibration_finalize       (GObject *object);
static gpointer     gwy_calibration_copy           (gpointer);
static void         gwy_calibration_use            (GwyResource *resource);
static void         gwy_calibration_release        (GwyResource *resource);
static void         gwy_calibration_sanitize       (GwyCalibration *calibration);
static void         gwy_calibration_dump           (GwyResource *resource,
                                                 GString *str);
static GwyResource* gwy_calibration_parse          (const gchar *text, 
                                                    gboolean is_const);


G_DEFINE_TYPE(GwyCalibration, gwy_calibration, GWY_TYPE_RESOURCE)

static void
gwy_calibration_class_init(GwyCalibrationClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_calibration_finalize;

    parent_class = GWY_RESOURCE_CLASS(gwy_calibration_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->item_type.copy = gwy_calibration_copy;

    res_class->name = "calibrations";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_set_default_item_name(res_class->inventory,
                                        GWY_CALIBRATION_DEFAULT);
    res_class->use = gwy_calibration_use;
    res_class->release = gwy_calibration_release;
    res_class->dump = gwy_calibration_dump;
    res_class->parse = gwy_calibration_parse;
}

static void
gwy_calibration_init(GwyCalibration *calibration)
{
    gwy_debug_objects_creation(G_OBJECT(calibration));
}

static void
gwy_calibration_finalize(GObject *object)
{
    GwyCalibration *calibration = (GwyCalibration*)object;

    g_free(calibration->filename);
    G_OBJECT_CLASS(gwy_calibration_parent_class)->finalize(object);
}

static void
gwy_calibration_use(GwyResource *resource)
{
    GwyCalibration *calibration;
    calibration = GWY_CALIBRATION(resource);
    /*here load the file*/
    
}

static void
gwy_calibration_release(GwyResource *resource)
{
    GwyCalibration *calibration;

    calibration = GWY_CALIBRATION(resource);
    
    /*here free the file*/
}

/**
 * gwy_calibration_get_npoints:
 * @calibration: A color calibration.
 *
 * Returns the number of points in a color calibration.
 *
 * Returns: The number of points in @calibration.
 **/
gint
gwy_calibration_get_ndata(GwyCalibration *calibration)
{
    g_return_val_if_fail(GWY_IS_CALIBRATION(calibration), 0);
    return calibration->ndata;
}




void
_gwy_calibration_class_setup_presets(void)
{
    GwyResourceClass *klass;

    klass = g_type_class_ref(GWY_TYPE_CALIBRATION);
}

GwyCalibration*
gwy_calibration_new(const gchar *name,
                 gint ndata,
                 const gchar *filename)
{
    GwyCalibration *calibration;
    gboolean is_const = 0;

    g_return_val_if_fail(name, NULL);

    calibration = g_object_new(GWY_TYPE_CALIBRATION, "is-const", is_const, NULL);
    calibration->ndata = ndata;
    if (filename) calibration->filename = g_strdup(filename);
    g_string_assign(GWY_RESOURCE(calibration)->name, name);

    GWY_RESOURCE(calibration)->is_modified = 0;

    return calibration;
}

static gpointer
gwy_calibration_copy(gpointer item)
{
    GwyCalibration *calibration, *copy;

    g_return_val_if_fail(GWY_IS_CALIBRATION(item), NULL);

    calibration = GWY_CALIBRATION(item);
    copy = gwy_calibration_new(gwy_resource_get_name(GWY_RESOURCE(item)),
                            calibration->ndata,
                            calibration->filename);

    return copy;
}

static void
gwy_calibration_dump(GwyResource *resource,
                  GString *str)
{
    GwyCalibration *calibration;

    g_return_if_fail(GWY_IS_CALIBRATION(resource));
    calibration = GWY_CALIBRATION(resource);
    g_string_append_printf(str, "npoints %u\nfilename %s\n", calibration->ndata, calibration->filename);
}

static GwyResource*
gwy_calibration_parse(const gchar *text,
                   gboolean is_const)
{
    GwyCalibration *calibration = NULL;
    GwyCalibrationClass *klass;
    guint ndata = 0;
    gchar *str, *p, *line, *key, *filename=NULL, *value;
    guint len;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_CALIBRATION);
    g_return_val_if_fail(klass, NULL);
   
    p = str = g_strdup(text);
    
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        key = line;
        if (!*key)
            continue;
        value = strchr(key, ' ');
        if (value) {
            *value = '\0';
            value++;
            g_strstrip(value);
        }
        if (!value || !*value) {
            g_warning("Missing value for `%s'.", key);
            continue;
        }

        /* Information */
        if (gwy_strequal(key, "npoints"))
            ndata = atoi(value);
        else if (gwy_strequal(key, "filename")) {
            len = strlen(value);
            filename = strdup(value);
        }
        else
            g_warning("Unknown field `%s'.", key);
    }
   
    calibration = gwy_calibration_new(filename,
                                ndata, filename);
    gwy_calibration_sanitize(calibration);
    return (GwyResource*)calibration;
}

static void  
gwy_calibration_sanitize(GwyCalibration *calibration)
{
}

/**
 * gwy_calibrations:
 *
 * Gets inventory with all the calibrations.
 *
 * Returns: Calibration inventory.
 **/
GwyInventory*
gwy_calibrations(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_CALIBRATION))->inventory;
}

/**
 * gwy_calibrations_get_calibration:
 * @name: Calibration name.  May be %NULL to get the default calibration.
 *
 * Convenience function to get a calibration from gwy_calibrations() by name.
 *
 * Returns: Calibration identified by @name or the default calibration if @name does
 *          not exist.
 **/
GwyCalibration*
gwy_calibrations_get_calibration(const gchar *name)
{
    GwyInventory *i;

    i = GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_CALIBRATION))->inventory;
    return (GwyCalibration*)gwy_inventory_get_item_or_default(i, name);
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
