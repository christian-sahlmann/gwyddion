/*
 *  @(#) $Id$
 *  Copyright (C) 2010,2011 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/gwycalibration.h>
#include "gwyprocessinternal.h"

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
    gchar *filename;
    GError *err = NULL;
    gchar *contents;
    gsize len, pos = 0;

    GwyCalibration *calibration;
    calibration = GWY_CALIBRATION(resource);

    if (calibration->caldata)
        g_object_unref(calibration->caldata);

    filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
   // printf("loading from %s\n", filename);
    if (!g_file_get_contents(filename,
                             &contents, &len, &err))
    {
        g_warning("Error loading file: %s\n", err->message);
        g_clear_error(&err);
        return;
    }
    else {
        if (len)
            calibration->caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
        g_free(contents);
    }

}

/**
 * gwy_calibration_get_data:
 * @calibration: A calibration.
 *
 * Obtains the data related to calibration.
 *
 * Returns: The data related to @calibration.
 *
 * Since: 2.23
 **/
GwyCalData*
gwy_calibration_get_data(GwyCalibration *calibration)
{
    return calibration->caldata;
}


static void
gwy_calibration_release(G_GNUC_UNUSED GwyResource *resource)
{
    /*
    GwyCalibration *calibration;

    calibration = GWY_CALIBRATION(resource);
    */

    /*here free the file*/
}

/**
 * gwy_calibration_get_npoints:
 * @calibration: A calibration.
 *
 * Returns the number of points in a calibration.
 *
 * Returns: The number of points in @calibration.
 *
 * Since: 2.23
 **/
gint
gwy_calibration_get_ndata(GwyCalibration *calibration)
{
    g_return_val_if_fail(GWY_IS_CALIBRATION(calibration), 0);
    if (calibration->caldata)
       return gwy_caldata_get_ndata(calibration->caldata);
    else return 0;
}




void
_gwy_calibration_class_setup_presets(void)
{
    g_type_class_ref(GWY_TYPE_CALIBRATION);
}

/**
 * gwy_calibration_new:
 * @name: Name of resource
 * @filename: Filename of associated calibration data
 *
 * Creates new calibration resource.
 *
 * Returns: A newly created calibration resource.
 *
 * Since: 2.23
 **/
GwyCalibration*
gwy_calibration_new(const gchar *name,
                 const gchar *filename)
{
    GwyCalibration *calibration;
    gboolean is_const = 0;

    g_return_val_if_fail(name, NULL);

    calibration = g_object_new(GWY_TYPE_CALIBRATION, "is-const", is_const, NULL);
    if (filename) calibration->filename = g_strdup(filename);
    g_string_assign(GWY_RESOURCE(calibration)->name, name);

    calibration->caldata = NULL;

    GWY_RESOURCE(calibration)->is_modified = 0;

    return calibration;
}

/**
 * gwy_calibration_get_filename:
 * @calibration: Calibration resource
 *
 * Get filename of associated calibration data.
 *
 * Returns: Filename of associated calibration data.
 *
 * Since: 2.23
 **/
const gchar*
gwy_calibration_get_filename(GwyCalibration *calibration)
{
    return calibration->filename;
}


static gpointer
gwy_calibration_copy(gpointer item)
{
    GwyCalibration *calibration, *copy;

    g_return_val_if_fail(GWY_IS_CALIBRATION(item), NULL);

    calibration = GWY_CALIBRATION(item);
    copy = gwy_calibration_new(gwy_resource_get_name(GWY_RESOURCE(item)),
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
    g_string_append_printf(str, "npoints %u\nfilename %s\n", gwy_calibration_get_ndata(calibration), calibration->filename);
}

static GwyResource*
gwy_calibration_parse(const gchar *text,
                      G_GNUC_UNUSED gboolean is_const)
{
    GwyCalibration *calibration = NULL;
    GwyCalibrationClass *klass;
    G_GNUC_UNUSED guint ndata = 0;
    gchar *str, *p, *line, *key, *filename=NULL, *value;

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
            filename = strdup(value);
        }
        else
            g_warning("Unknown field `%s'.", key);
    }

    calibration = gwy_calibration_new(filename, filename);
    gwy_calibration_sanitize(calibration);
    return (GwyResource*)calibration;
}

static void
gwy_calibration_sanitize(G_GNUC_UNUSED GwyCalibration *calibration)
{
}

/**
 * gwy_calibrations:
 *
 * Gets inventory with all the calibrations.
 *
 * Returns: Calibration inventory.
 *
 * Since: 2.23
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
 * Returns: Calibration identified by @name or the default calibration if @name
 *          does not exist.
 *
 * Since: 2.23
 **/
GwyCalibration*
gwy_calibrations_get_calibration(const gchar *name)
{
    GwyInventory *i;

    i = GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_CALIBRATION))->inventory;
    return (GwyCalibration*)gwy_inventory_get_item_or_default(i, name);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwycalibration
 * @title: GwyCalibration
 * @short_description: Resource for managing calibrations
 *
 * #GwyCalibration is a resource used for managing calibration data. These
 * resources are stored separately from calibration data and contain namely
 * filename of connected calibration data file.
 **/

/**
 * GwyCalibration:
 *
 * The #GwyCalibration struct contains private data only and should be accessed
 * using the functions below.
 *
 * Since: 2.23
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
