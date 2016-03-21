/*
 *  $Id$
 *  Copyright (C) 2011-2016 David Nečas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwymd5.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/surface.h>
#include <libprocess/stats.h>
#include "gwyprocessinternal.h"

#define GWY_SURFACE_TYPE_NAME "GwySurface"

#define gwy_assign(dest, source, n) \
    memcpy((dest), (source), (n)*sizeof((dest)[0]))

enum {
    DATA_CHANGED,
    N_SIGNALS
};

static void        gwy_surface_finalize         (GObject *object);
static void        gwy_surface_serializable_init(GwySerializableIface *iface);
static void        gwy_surface_dispose          (GObject *object);
static GByteArray* gwy_surface_serialize        (GObject *obj,
                                                 GByteArray *buffer);
static gsize       gwy_surface_get_size         (GObject *obj);
static GObject*    gwy_surface_deserialize      (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject*    gwy_surface_duplicate_real   (GObject *serializable);
static void        gwy_surface_clone_real       (GObject *destination,
                                                 GObject *source);
static void        copy_info                    (GwySurface *dest,
                                                 const GwySurface *src);
static void        ensure_ranges                (GwySurface *surface);
static void        ensure_checksum              (GwySurface *surface);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwySurface, gwy_surface, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_surface_serializable_init));

static void
gwy_surface_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_surface_serialize;
    iface->deserialize = gwy_surface_deserialize;
    iface->get_size = gwy_surface_get_size;
    iface->duplicate = gwy_surface_duplicate_real;
    iface->clone = gwy_surface_clone_real;
}

static void
gwy_surface_class_init(GwySurfaceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Surface));

    gobject_class->dispose = gwy_surface_dispose;
    gobject_class->finalize = gwy_surface_finalize;

    /**
     * GwySurface::data-changed:
     * @gwysurface: The #GwySurface which received the signal.
     *
     * The ::data-changed signal is never emitted by surface itself.  It
     * is intended as a means to notify others data field users they should
     * update themselves.
     **/
    signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySurfaceClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_surface_init(GwySurface *surface)
{
    surface->priv = G_TYPE_INSTANCE_GET_PRIVATE(surface,
                                                GWY_TYPE_SURFACE,
                                                Surface);
}

static void
alloc_data(GwySurface *surface)
{
    g_free(surface->data);
    surface->data = NULL;;
    if (surface->n)
        surface->data = g_new(GwyXYZ, surface->n);
}

static void
free_data(GwySurface *surface)
{
    g_free(surface->data);
    surface->data = NULL;;
}

static void
ensure_units(GwySurface *surface)
{
    Surface *priv = surface->priv;

    if (!priv->si_unit_xy)
        priv->si_unit_xy = gwy_si_unit_new(NULL);
    if (!priv->si_unit_z)
        priv->si_unit_z = gwy_si_unit_new(NULL);
}

static void
gwy_surface_finalize(GObject *object)
{
    GwySurface *surface = GWY_SURFACE(object);
    free_data(surface);
    G_OBJECT_CLASS(gwy_surface_parent_class)->finalize(object);
}

static void
gwy_surface_dispose(GObject *object)
{
    GwySurface *surface = GWY_SURFACE(object);
    gwy_object_unref(surface->priv->si_unit_xy);
    gwy_object_unref(surface->priv->si_unit_z);
    G_OBJECT_CLASS(gwy_surface_parent_class)->dispose(object);
}

static GByteArray*
gwy_surface_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySurface *surface;
    guint32 datasize;

    g_return_val_if_fail(GWY_IS_SURFACE(obj), NULL);

    surface = GWY_SURFACE(obj);
    ensure_units(surface);
    datasize = 3*surface->n;
    {
        GwySerializeSpec spec[] = {
            { 'o', "si_unit_xy", &surface->priv->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &surface->priv->si_unit_z, NULL, },
            { 'D', "data", &surface->data, &datasize, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SURFACE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_surface_get_size(GObject *obj)
{
    GwySurface *surface;
    guint32 datasize;

    g_return_val_if_fail(GWY_IS_SURFACE(obj), 0);

    surface = GWY_SURFACE(obj);
    ensure_units(surface);
    datasize = 3*surface->n;
    {
        GwySerializeSpec spec[] = {
            { 'o', "si_unit_xy", &surface->priv->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &surface->priv->si_unit_z, NULL, },
            { 'D', "data", &surface->data, &datasize, },
        };
        return gwy_serialize_get_struct_size(GWY_SURFACE_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_surface_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    guint32 datasize;
    GwySIUnit *si_unit_xy = NULL, *si_unit_z = NULL;
    GwySurface *surface;
    GwyXYZ *data;
    GwySerializeSpec spec[] = {
        { 'o', "si_unit_xy", &si_unit_xy, NULL, },
        { 'o', "si_unit_z", &si_unit_z, NULL, },
        { 'D', "data", &data, &datasize, },
    };

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SURFACE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }
    if (datasize % 3 != 0) {
        g_critical("Serialized %s data size %u not a multiple of 3",
                   GWY_SURFACE_TYPE_NAME, datasize);
        g_free(data);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }

    surface = gwy_surface_new();

    g_free(surface->data);
    surface->data = data;
    surface->n = datasize/3;

    if (si_unit_z) {
        gwy_object_unref(surface->priv->si_unit_z);
        surface->priv->si_unit_z = si_unit_z;
    }
    if (si_unit_xy) {
        gwy_object_unref(surface->priv->si_unit_xy);
        surface->priv->si_unit_xy = si_unit_xy;
    }

    return (GObject*)surface;
}

static GObject*
gwy_surface_duplicate_real(GObject *object)
{
    GwySurface *surface, *duplicate;

    g_return_val_if_fail(GWY_IS_SURFACE(object), NULL);
    surface = GWY_SURFACE(object);
    duplicate = gwy_surface_new_from_data(surface->data, surface->n);
    copy_info(duplicate, surface);

    return (GObject*)duplicate;
}

static void
gwy_surface_clone_real(GObject *source, GObject *copy)
{
    GwySurface *surface, *clone;

    g_return_if_fail(GWY_IS_SURFACE(source));
    g_return_if_fail(GWY_IS_SURFACE(copy));

    surface = GWY_SURFACE(source);
    clone = GWY_SURFACE(copy);

    if (clone->n != surface->n) {
        clone->n = surface->n;
        alloc_data(clone);
    }
    gwy_assign(clone->data, surface->data, surface->n);
    copy_info(clone, surface);
}

static void
copy_info(GwySurface *dest,
          const GwySurface *src)
{
    Surface *dpriv = dest->priv, *spriv = src->priv;

    /* SI Units can be NULL */
    if (spriv->si_unit_xy && dpriv->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(spriv->si_unit_xy),
                               G_OBJECT(dpriv->si_unit_xy));
    else if (spriv->si_unit_xy && !dpriv->si_unit_xy)
        dpriv->si_unit_xy = gwy_si_unit_duplicate(spriv->si_unit_xy);
    else if (!spriv->si_unit_xy && dpriv->si_unit_xy)
        gwy_object_unref(dpriv->si_unit_xy);

    if (spriv->si_unit_z && dpriv->si_unit_z)
        gwy_serializable_clone(G_OBJECT(spriv->si_unit_z),
                               G_OBJECT(dpriv->si_unit_z));
    else if (spriv->si_unit_z && !dpriv->si_unit_z)
        dpriv->si_unit_z = gwy_si_unit_duplicate(spriv->si_unit_z);
    else if (!spriv->si_unit_z && dpriv->si_unit_z)
        gwy_object_unref(dpriv->si_unit_z);

    dpriv->cached_ranges = spriv->cached_ranges;
    dpriv->min = spriv->min;
    dpriv->max = spriv->max;

    dpriv->cached_checksum = spriv->cached_checksum;
    gwy_assign(dpriv->checksum, spriv->checksum, G_N_ELEMENTS(spriv->checksum));
}

/**
 * gwy_surface_new:
 *
 * Creates a new empty surface.
 *
 * The surface will not contain any points. This parameterless constructor
 * exists mainly for language bindings, gwy_surface_new_from_data() is usually
 * more useful.
 *
 * Returns: A new empty surface.
 *
 * Since: 2.45
 **/
GwySurface*
gwy_surface_new(void)
{
    return g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
}

/**
 * gwy_surface_new_sized:
 * @n: Number of points.
 *
 * Creates a new surface with preallocated size.
 *
 * The surface will contain the speficied number of points with uninitialised
 * values.
 *
 * Returns: A new surface.
 *
 * Since: 2.45
 **/
GwySurface*
gwy_surface_new_sized(guint n)
{
    GwySurface *surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    surface->n = n;
    alloc_data(surface);
    return surface;
}

/**
 * gwy_surface_new_from_data:
 * @points: Array of @n points with the surface data.
 * @n: Number of points.
 *
 * Creates a new surface, filling it with provided points.
 *
 * Returns: A new surface.
 *
 * Since: 2.45
 **/
GwySurface*
gwy_surface_new_from_data(const GwyXYZ *points,
                          guint n)
{
    GwySurface *surface;

    g_return_val_if_fail(!n || points, NULL);

    surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    surface->n = n;
    alloc_data(surface);
    gwy_assign(surface->data, points, n);
    return surface;
}

/**
 * gwy_surface_new_alike:
 * @model: A surface to use as the template.
 *
 * Creates a new empty surface similar to another surface.
 *
 * The units of the new surface will be identical to those of @model but the
 * new surface will not contain any points.  Use gwy_surface_duplicate() to
 * completely duplicate a surface including data.
 *
 * Returns: A new empty surface.
 *
 * Since: 2.45
 **/
GwySurface*
gwy_surface_new_alike(GwySurface *model)
{
    GwySurface *surface;

    g_return_val_if_fail(GWY_IS_SURFACE(model), NULL);

    surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);
    copy_info(surface, model);
    gwy_surface_invalidate(surface);
    return surface;
}

/**
 * gwy_surface_new_part:
 * @surface: A surface.
 * @xfrom: Minimum x-coordinate value.
 * @xto: Maximum x-coordinate value.
 * @yfrom: Minimum y-coordinate value.
 * @yto: Maximum y-coordinate value.
 *
 * Creates a new surface as a part of another surface.
 *
 * The new surface consits of data with lateral coordinates within the
 * specified ranges (inclusively).  It may be empty.
 *
 * Data are physically copied, i.e. changing the new surface data does not
 * change @surface's data and vice versa.
 *
 * Returns: A new surface.
 *
 * Since: 2.45
 **/
GwySurface*
gwy_surface_new_part(GwySurface *surface,
                     gdouble xfrom,
                     gdouble xto,
                     gdouble yfrom,
                     gdouble yto)
{
    GwySurface *part;
    guint n, i;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    part = gwy_surface_new_alike(surface);
    if (!surface->n || xfrom > xto || yfrom > yto)
        return part;

    n = 0;
    for (i = 0; i < surface->n; i++) {
        gdouble x = surface->data[i].x, y = surface->data[i].y;
        if (x >= xfrom && x <= xto && y >= yfrom && y <= yto)
            n++;
    }
    part->n = n;
    alloc_data(part);

    n = 0;
    for (i = 0; i < surface->n; i++) {
        gdouble x = surface->data[i].x, y = surface->data[i].y;
        if (x >= xfrom && x <= xto && y >= yfrom && y <= yto)
            part->data[n++] = surface->data[i];
    }

    return part;
}

static void
copy_field_to_surface(GwyDataField *field,
                      GwySurface *surface)
{
    gdouble dx = gwy_data_field_get_xmeasure(field),
            dy = gwy_data_field_get_ymeasure(field);
    gdouble xoff = 0.5*dx + field->xoff, yoff = 0.5*dy + field->yoff;
    Surface *priv = surface->priv;
    guint i, j, k = 0;

    for (i = 0; i < field->yres; i++) {
        for (j = 0; j < field->xres; j++) {
            surface->data[k].x = dx*j + xoff;
            surface->data[k].y = dy*i + yoff;
            surface->data[k].z = field->data[k];
            k++;
        }
    }

    /* SI Units can be NULL */
    if (field->si_unit_xy && priv->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(field->si_unit_xy),
                               G_OBJECT(priv->si_unit_xy));
    else if (field->si_unit_xy && !priv->si_unit_xy)
        priv->si_unit_xy = gwy_si_unit_duplicate(field->si_unit_xy);
    else if (!field->si_unit_xy && priv->si_unit_xy)
        gwy_object_unref(priv->si_unit_xy);

    if (field->si_unit_z && priv->si_unit_z)
        gwy_serializable_clone(G_OBJECT(field->si_unit_z),
                               G_OBJECT(priv->si_unit_z));
    else if (field->si_unit_z && !priv->si_unit_z)
        priv->si_unit_z = gwy_si_unit_duplicate(field->si_unit_z);
    else if (!field->si_unit_z && priv->si_unit_z)
        gwy_object_unref(priv->si_unit_z);

    gwy_surface_invalidate(surface);
    priv->cached_ranges = TRUE;
    priv->min.x = xoff;
    priv->min.y = yoff;
    priv->max.x = dx*(field->xres - 1) + xoff;
    priv->max.y = dy*(field->yres - 1) + yoff;
    gwy_data_field_get_min_max(field, &priv->min.z, &priv->max.z);
}

/**
 * gwy_surface_get_data:
 * @surface: A surface.
 *
 * Gets the raw XYZ data array of a surface.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * surface life time.
 *
 * This function invalidates any cached information, use
 * gwy_surface_get_data_const() if you are not going to change the data.
 *
 * See gwy_surface_invalidate() for some discussion.
 *
 * Returns: The surface XYZ data as a pointer to an array of
 *          gwy_surface_get_npoints() items.
 *
 * Since: 2.45
 **/
GwyXYZ*
gwy_surface_get_data(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    gwy_surface_invalidate(surface);
    return surface->data;
}

/**
 * gwy_surface_get_data_const:
 * @surface: A surface.
 *
 * Gets the raw XYZ data array of a surface, read-only.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * field life time.
 *
 * Use gwy_surface_get_data() if you want to change the data.
 *
 * See gwy_surface_invalidate() for some discussion.
 *
 * Returns: The surface XYZ data as a pointer to an array of
 *          gwy_surface_get_npoints() items.
 *
 * Since: 2.45
 **/
const GwyXYZ*
gwy_surface_get_data_const(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    return surface->data;
}

/**
 * gwy_surface_get_npoints:
 * @surface: A surface.
 *
 * Gets the number of points in an XYZ surface.
 *
 * Returns: The number of points.
 *
 * Since: 2.45
 **/
guint
gwy_surface_get_npoints(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), 0);
    return surface->n;
}

/**
 * gwy_surface_data_changed:
 * @surface: A surface.
 *
 * Emits signal GwySurface::data-changed on a surface.
 *
 * Since: 2.45
 **/
void
gwy_surface_data_changed(GwySurface *surface)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_signal_emit(surface, signals[DATA_CHANGED], 0);
}

/**
 * gwy_surface_copy:
 * @src: Source surface.
 * @dest: Destination surface.
 *
 * Copies the data of a surface to another surface of the same dimensions.
 *
 * Only the data points are copied.  To make a surface completely identical to
 * another, including units and change of dimensions, you can use
 * gwy_surface_clone().
 *
 * Since: 2.45
 **/
void
gwy_surface_copy(GwySurface *src,
                 GwySurface *dest)
{
    g_return_if_fail(GWY_IS_SURFACE(src));
    g_return_if_fail(GWY_IS_SURFACE(dest));
    g_return_if_fail(dest->n == src->n);
    gwy_assign(dest->data, src->data, src->n);
    gwy_surface_invalidate(dest);
}

/**
 * gwy_surface_invalidate:
 * @surface: A surface.
 *
 * Invalidates cached surface statistics.
 *
 * Cached statistics include ranges returned by gwy_surface_get_xrange(),
 * gwy_surface_get_yrange() and gwy_surface_get_min_max(), the fingerprint
 * for gwy_surface_xy_is_compatible() and and possibly other characteristics
 * in the future.
 *
 * See gwy_data_field_invalidate() for discussion of invalidation and examples.
 *
 * Since: 2.45
 **/
void
gwy_surface_invalidate(GwySurface *surface)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    surface->priv->cached_ranges = FALSE;
    surface->priv->cached_checksum = FALSE;
}

/**
 * gwy_surface_set_from_data_field:
 * @surface: A surface.
 * @data_field: A two-dimensional data field.
 *
 * Sets the data and units of a surface from a field.
 *
 * The number of points in the new surface will be equal to the number of
 * points in the field.  Lateral coordinates will be equal to the corresponding
 * @data_field coordinates; values will be created in regular grid according to
 * @data_field's physical size and offset.
 *
 * Lateral and value units will correspond to @data_field's units.  This means
 * the field needs to have identical @x and @y units.
 *
 * Since: 2.45
 **/
void
gwy_surface_set_from_data_field(GwySurface *surface,
                                GwyDataField *dfield)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));

    if (surface->n != dfield->xres * dfield->yres) {
        free_data(surface);
        surface->n = dfield->xres * dfield->yres;
        alloc_data(surface);
    }
    copy_field_to_surface(dfield, surface);
}

/**
 * gwy_surface_get_si_unit_xy:
 * @surface: A surface.
 *
 * Returns lateral SI unit of a surface.
 *
 * Returns: SI unit corresponding to the lateral (XY) dimensions of the
 *          surface. Its reference count is not incremented.
 *
 * Since: 2.45
 **/
GwySIUnit*
gwy_surface_get_si_unit_xy(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    if (!surface->priv->si_unit_xy)
        surface->priv->si_unit_xy = gwy_si_unit_new(NULL);

    return surface->priv->si_unit_xy;
}

/**
 * gwy_surface_get_si_unit_z:
 * @surface: A surface.
 *
 * Returns value SI unit of a surface.
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of the surface.
 *          Its reference count is not incremented.
 *
 * Since: 2.45
 **/
GwySIUnit*
gwy_surface_get_si_unit_z(GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    if (!surface->priv->si_unit_z)
        surface->priv->si_unit_z = gwy_si_unit_new(NULL);

    return surface->priv->si_unit_z;
}

/**
 * gwy_surface_set_si_unit_xy:
 * @surface: A surface.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (XY) dimensions of a surface.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.45
 **/
void
gwy_surface_set_si_unit_xy(GwySurface *surface,
                           GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (surface->priv->si_unit_xy == si_unit)
        return;

    gwy_object_unref(surface->priv->si_unit_xy);
    g_object_ref(si_unit);
    surface->priv->si_unit_xy = si_unit;
}

/**
 * gwy_surface_set_si_unit_z:
 * @surface: A surface.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of a surface.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.45
 **/
void
gwy_surface_set_si_unit_z(GwySurface *surface,
                          GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (surface->priv->si_unit_z == si_unit)
        return;

    gwy_object_unref(surface->priv->si_unit_z);
    g_object_ref(si_unit);
    surface->priv->si_unit_z = si_unit;
}

/**
 * gwy_surface_get_value_format_xy:
 * @surface: A surface.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of a surface.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.45
 **/
GwySIValueFormat*
gwy_surface_get_value_format_xy(GwySurface *surface,
                                GwySIUnitFormatStyle style,
                                GwySIValueFormat *format)
{
    gdouble xmin, xmax, ymin, ymax;
    gdouble max, unit;
    GwySIUnit *siunit;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    siunit = gwy_surface_get_si_unit_xy(surface);
    if (!surface->n) {
        return gwy_si_unit_get_format_with_resolution(siunit, style, 1.0, 0.1,
                                                      format);
    }

    if (surface->n == 1) {
        max = MAX(fabs(surface->data[0].x), fabs(surface->data[0].y));
        if (!max)
            max = 1.0;
        return gwy_si_unit_get_format_with_resolution(siunit,
                                                      style, max, max/10.0,
                                                      format);
    }

    gwy_surface_get_xrange(surface, &xmin, &xmax);
    gwy_surface_get_yrange(surface, &ymin, &ymax);
    max = MAX(MAX(fabs(xmax), fabs(xmin)), MAX(fabs(ymax), fabs(ymin)));
    if (!max)
        max = 1.0;
    unit = hypot(ymax - ymin, xmax - xmin)/sqrt(surface->n);
    if (!unit)
        unit = max/10.0;

    return gwy_si_unit_get_format_with_resolution(siunit, style, max, 0.2*unit,
                                                  format);
}

/**
 * gwy_surface_get_value_format_z:
 * @surface: A surface.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a surface.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.45
 **/
GwySIValueFormat*
gwy_surface_get_value_format_z(GwySurface *surface,
                               GwySIUnitFormatStyle style,
                               GwySIValueFormat *format)
{
    GwySIUnit *siunit;
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    siunit = gwy_surface_get_si_unit_z(surface);
    gwy_surface_get_min_max(surface, &min, &max);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }

    return gwy_si_unit_get_format_with_digits(siunit, style, max - min, 3,
                                              format);
}

/**
 * gwy_surface_get:
 * @surface: A surface.
 * @pos: Position in @surface.
 *
 * Obtains a single surface point.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to simply accessing @data in #GwySurface directly in
 * C.
 *
 * Returns: The point at @pos.
 *
 * Since: 2.45
 **/
GwyXYZ
gwy_surface_get(GwySurface *surface,
                guint pos)
{
    GwyXYZ nullpoint = { 0.0, 0.0, 0.0 };

    g_return_val_if_fail(GWY_IS_SURFACE(surface), nullpoint);
    g_return_val_if_fail(pos < surface->n, nullpoint);
    return surface->data[pos];
}

/**
 * gwy_surface_set:
 * @surface: A surface.
 * @pos: Position in @surface.
 * @point: Point to store at given position.
 *
 * Sets a single surface value.
 *
 * This function exists <emphasis>only for language bindings</emphasis> as it
 * is very slow compared to simply accessing @data in #GwySurface directly in
 * C.
 *
 * Since: 2.45
 **/
void
gwy_surface_set(GwySurface *surface,
                guint pos,
                GwyXYZ point)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(pos < surface->n);
    surface->data[pos] = point;
}

/**
 * gwy_surface_get_data_full:
 * @surface: A surface.
 * @n: Location to store the count of extracted data points.
 *
 * Provides the values of an entire surface as a flat array.
 *
 * This function, paired with gwy_surface_set_data_full() can be namely useful
 * in language bindings.
 *
 * Note that this function returns a pointer directly to @surface's data.
 *
 * Returns: The array containing the surface points.
 *
 * Since: 2.45
 **/
const GwyXYZ*
gwy_surface_get_data_full(GwySurface *surface,
                          guint *n)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    *n = surface->n;
    return surface->data;
}

/**
 * gwy_surface_set_data_full:
 * @surface: A surface.
 * @points: Data points to copy to the surface.  They replace whatever points
 *          are in @surface now.
 * @n: The number of points in @data.
 *
 * Puts back values from a flat array to an entire data surface.
 *
 * See gwy_surface_get_data_full() for a discussion.
 *
 * Since: 2.45
 **/
void
gwy_surface_set_data_full(GwySurface *surface,
                          const GwyXYZ *points,
                          guint n)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(points || !n);
    if (surface->n != n) {
        surface->n = n;
        alloc_data(surface);
    }
    if (n)
        gwy_assign(surface->data, points, n);
}

/**
 * gwy_surface_get_xrange:
 * @surface: A surface.
 * @min: Location where to store the minimum value.
 * @max: Location where to store the maximum value.
 *
 * Gets the range of X coordinates of a surface.
 *
 * This information is cached.
 *
 * Since: 2.45
 **/
void
gwy_surface_get_xrange(GwySurface *surface,
                       gdouble *min,
                       gdouble *max)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    ensure_ranges(surface);
    if (min)
        *min = surface->priv->min.x;
    if (max)
        *max = surface->priv->max.x;
}

/**
 * gwy_surface_get_yrange:
 * @surface: A surface.
 * @min: Location where to store the minimum value.
 * @max: Location where to store the maximum value.
 *
 * Gets the range of Y coordinates of a surface.
 *
 * This information is cached.
 *
 * Since: 2.45
 **/
void
gwy_surface_get_yrange(GwySurface *surface,
                       gdouble *min,
                       gdouble *max)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    ensure_ranges(surface);
    if (min)
        *min = surface->priv->min.y;
    if (max)
        *max = surface->priv->max.y;
}

/**
 * gwy_surface_get_min_max:
 * @surface: A surface.
 * @min: Location where to store the minimum value.
 * @max: Location where to store the maximum value.
 *
 * Gets the range of Z values of a surface.
 *
 * This information is cached.
 *
 * Since: 2.45
 **/
void
gwy_surface_get_min_max(GwySurface *surface,
                        gdouble *min,
                        gdouble *max)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    ensure_ranges(surface);
    if (min)
        *min = surface->priv->min.z;
    if (max)
        *max = surface->priv->max.z;
}

static void
ensure_ranges(GwySurface *surface)
{
    Surface *priv = surface->priv;
    GwyXYZ min, max;
    guint i;

    if (priv->cached_ranges)
        return;

    priv->cached_ranges = TRUE;
    if (!surface->n) {
        gwy_clear(&priv->min, 1);
        gwy_clear(&priv->max, 1);
        return;
    }

    min = max = surface->data[0];
    for (i = 1; i < surface->n; i++) {
        GwyXYZ pt = surface->data[i];

        if (pt.x > max.x)
            max.x = pt.x;
        if (pt.x < min.x)
            min.x = pt.x;

        if (pt.y > max.y)
            max.y = pt.y;
        if (pt.y < min.y)
            min.y = pt.y;

        if (pt.z > max.z)
            max.z = pt.z;
        if (pt.z < min.z)
            min.z = pt.z;
    }

    priv->min = min;
    priv->max = max;
    return;
}

/**
 * gwy_surface_xy_is_compatible:
 * @surface: A surface.
 * @othersurface: Another surface.
 *
 * Checks whether the XY positions of two surfaces are compatible.
 *
 * Compatible XY positions mean the XY units are the same and the points are
 * the same.  Two surfaces that have the same set of XY points but in different
 * orders are <emphasis>not</emphasis> considered compatible.  This is because
 * the points at the same index in @data are different and thus calculations
 * involving data from the two surfaces are impossible.  It is necessary to
 * match the points order in the two surfaces to make this possible.
 *
 * This information is cached.
 *
 * Returns: %TRUE if the surfaces are XY-position compatible, %FALSE if they
 *          are not.
 *
 * Since: 2.45
 **/
gboolean
gwy_surface_xy_is_compatible(GwySurface *surface,
                             GwySurface *othersurface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), FALSE);
    g_return_val_if_fail(GWY_IS_SURFACE(othersurface), FALSE);

    ensure_units(surface);
    ensure_units(othersurface);
    if (!gwy_si_unit_equal(surface->priv->si_unit_xy,
                           othersurface->priv->si_unit_xy))
        return FALSE;

    ensure_checksum(surface);
    ensure_checksum(othersurface);
    return !memcmp(surface->priv->checksum, othersurface->priv->checksum,
                   sizeof(surface->priv->checksum));
}

static void
ensure_checksum(GwySurface *surface)
{
    gdouble *xydata;
    guint i, n, k;

    if (surface->priv->cached_checksum)
        return;

    n = surface->n;
    xydata = g_new(gdouble, 2*n);
    for (i = k = 0; i < n; i++) {
        xydata[k++] = surface->data[i].x;
        xydata[k++] = surface->data[i].y;
    }
    gwy_md5_get_digest((gchar*)xydata, 2*n*sizeof(gdouble),
                       surface->priv->checksum);
    g_free(xydata);

    surface->priv->cached_checksum = TRUE;
}

/**
 * SECTION: surface
 * @title: GwySurface
 * @short_description: General two-dimensional data
 *
 * #GwySurface represents general, i.e. possibly unevenly spaced,
 * two-dimensional data, also called XYZ data.
 *
 * Surface points are stored in a flat array #GwySurface-struct.data of #GwyXYZ
 * values.
 *
 * Unlike #GwyDataField, a surface can also be empty, i.e. contain zero points.
 **/

/**
 * GwySurface:
 * @n: Number of points.
 * @data: Surface data.  See the introductory section for details.
 *
 * Object representing surface data.
 *
 * The #GwySurface struct contains some public fields that can be directly
 * accessed for reading.  To set them, you must use the #GwySurface methods.
 *
 * Since: 2.45
 **/

/**
 * GwySurfaceClass:
 *
 * Class of surfaces.
 *
 * Since: 2.45
 **/

/**
 * gwy_surface_duplicate:
 * @surface: A surface.
 *
 * Duplicates a surface.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 *
 * Since: 2.45
 **/

/**
 * gwy_surface_clone:
 * @dest: Destination surface.
 * @src: Source surface.
 *
 * Copies the value of a surface.
 *
 * This is a convenience wrapper of gwy_serializable_clone().
 *
 * Since: 2.45
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
