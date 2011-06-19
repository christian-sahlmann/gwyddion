/*
 *  @(#) $Id: surface.c 10570 2009-11-29 10:28:22Z klapetek $
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
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/surface.h>
#include <libprocess/surface-statistics.h>
#include <libprocess/interpolation.h>
#include <libprocess/stats.h>
#include "gwyprocessinternal.h"

#define GWY_SURFACE_TYPE_NAME "GwySurface"

enum { N_ITEMS = 3 };

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_N_POINTS,
    PROP_UNIT_XY,
    PROP_UNIT_Z,
    N_PROPS
};

static void       gwy_surface_finalize         (GObject *object);
static void       gwy_surface_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_surface_serialize       (GObject *obj,
                                                   GByteArray *buffer);
static gsize      gwy_surface_get_size         (GObject *obj);
static GObject*   gwy_surface_deserialize      (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*   gwy_surface_duplicate_real   (GObject *object);
static void       gwy_surface_clone_real       (GObject *source,
                                                   GObject *copy);

static guint surface_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwySurface, gwy_surface, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_surface_serializable_init))

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

    gobject_class->finalize = gwy_surface_finalize;

/**
 * GwySurface::data-changed:
 * @gwysurface: The #GwySurface which received the signal.
 *
 * The ::data-changed signal is never emitted by data field itself.  It
 * is intended as a means to notify others data field users they should
 * update themselves.
 */
    surface_signals[DATA_CHANGED]
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
    gwy_debug_objects_creation(G_OBJECT(surface));
}

static void
gwy_surface_finalize(GObject *object)
{
    GwySurface *surface = (GwySurface*)object;

    gwy_object_unref(surface->si_unit_xy);
    gwy_object_unref(surface->si_unit_z);

    g_free(surface->data);

    G_OBJECT_CLASS(gwy_surface_parent_class)->finalize(object);
}

/**
 * gwy_surface_new:
 * @xres: X-resolution, i.e., the number of columns.
 * @yres: Y-resolution, i.e., the number of rows.
 * @xreal: Real horizontal physical dimension.
 * @yreal: Real vertical physical dimension.
 * @nullme: Whether the data field should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data field.
 *
 * Returns: A newly created surface.
 **/
GwySurface*
gwy_surface_new (gdouble xmin,
                 gdouble xmax,
                 gdouble ymin,
                 gdouble ymax,
                 guint32 n,
                 gboolean nullme)
{
    GwySurface *surface;

    surface = g_object_new(GWY_TYPE_SURFACE, NULL);

    surface->xmin = xmin;
    surface->xmax = xmax;
    surface->ymin = ymin;
    surface->ymax = ymax;
    surface->n = n;
    if (nullme) {
        surface->data = (GwyXYZ*)g_new0(gdouble, 3*surface->n);

        surface->cached = GWY_SURFACE_CBIT(MIN) | GWY_SURFACE_CBIT(MAX) | GWY_SURFACE_CBIT(AVG) |
                          GWY_SURFACE_CBIT(RMS) | GWY_SURFACE_CBIT(MED);

    }
    else {
        surface->data = (GwyXYZ*)g_new(gdouble, 3*surface->n);
    }

    return surface;
}

GwySurface*
gwy_surface_new_alike (const GwySurface *model,
                       gboolean nullme)
{
    GwySurface *surface;

    g_return_val_if_fail(GWY_IS_SURFACE(model), NULL);
    surface = g_object_new(GWY_TYPE_SURFACE, NULL);

    surface->xmax = model->xmax;
    surface->ymax = model->ymax;
    surface->xmin = model->xmin;
    surface->ymin = model->ymin;
    surface->n = model->n;

    if (nullme) {
        surface->data = g_new0(GwyXYZ, surface->n);

        surface->cached = GWY_SURFACE_CBIT(MIN) | GWY_SURFACE_CBIT(MAX) | GWY_SURFACE_CBIT(AVG) |
                          GWY_SURFACE_CBIT(RMS) | GWY_SURFACE_CBIT(MED);

    }
    else
        surface->data = g_new(GwyXYZ, surface->n);

    if (model->si_unit_xy)
        surface->si_unit_xy = gwy_si_unit_duplicate(model->si_unit_xy);
    if (model->si_unit_z)
        surface->si_unit_z = gwy_si_unit_duplicate(model->si_unit_z);

    return surface;
}

static GByteArray*
gwy_surface_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySurface *surface;
    guint32 datasize;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SURFACE(obj), NULL);

    surface = GWY_SURFACE(obj);
    if (!surface->si_unit_xy)
        surface->si_unit_xy = gwy_si_unit_new("");
    if (!surface->si_unit_z)
        surface->si_unit_z = gwy_si_unit_new("");
    datasize = 3*surface->n;
    {
        GwySerializeSpec spec[] = {
            { 'd', "xmin", &surface->xmin, NULL, },
            { 'd', "xmax", &surface->xmax, NULL, },
            { 'd', "ymin", &surface->ymin, NULL, },
            { 'd', "ymax", &surface->ymax, NULL, },
            { 'o', "si_unit_xy", &surface->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &surface->si_unit_z, NULL, },
            { 'i', "n", &surface->n, NULL, },
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

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SURFACE(obj), 0);

    surface = GWY_SURFACE(obj);
    if (!surface->si_unit_xy)
        surface->si_unit_xy = gwy_si_unit_new("");
    if (!surface->si_unit_z)
        surface->si_unit_z = gwy_si_unit_new("");
    datasize = 3*surface->n;

    {
        GwySerializeSpec spec[] = {
            { 'd', "xmin", &surface->xmin, NULL, },
            { 'd', "xmax", &surface->xmax, NULL, },
            { 'd', "ymin", &surface->ymin, NULL, },
            { 'd', "ymax", &surface->ymax, NULL, },
            { 'o', "si_unit_xy", &surface->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &surface->si_unit_z, NULL, },
            { 'i', "n", &surface->n, NULL, },
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
    gdouble xmin, xmax, ymin, ymax;
    GwySIUnit *si_unit_xy = NULL, *si_unit_z = NULL;
    gint n;
    gdouble *data = NULL;
    GwySurface *surface;


    GwySerializeSpec spec[] = {
            { 'd', "xmin", &xmin, NULL, },
            { 'd', "xmax", &xmax, NULL, },
            { 'd', "ymin", &ymin, NULL, },
            { 'd', "ymax", &ymax, NULL, },
            { 'o', "si_unit_xy", &si_unit_xy, NULL, },
            { 'o', "si_unit_z", &si_unit_z, NULL, },
            { 'i', "n", &n, NULL, },
            { 'D', "data", &data, &datasize},
    };

    gwy_debug("");


    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SURFACE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {


        g_free(data);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }
    if (datasize != (gsize)(3*n)) {
        g_critical("Serialized %s size mismatch %u != 3*%u",
                   GWY_SURFACE_TYPE_NAME, datasize, n);
        g_free(data);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    surface = gwy_surface_new(0,0,0,0,1, FALSE);
    g_free(surface->data);
    surface->data = (GwyXYZ*)data;
    surface->n    = n;
    surface->xmin    = xmin;
    surface->xmax    = xmax;
    surface->ymin    = ymin;
    surface->ymax    = ymax;
    if (si_unit_z) {
        gwy_object_unref(surface->si_unit_z);
        surface->si_unit_z = si_unit_z;
    }
    if (si_unit_xy) {
        gwy_object_unref(surface->si_unit_xy);
        surface->si_unit_xy = si_unit_xy;
    }
    return (GObject*)surface;
}

static GObject*
gwy_surface_duplicate_real(GObject *object)
{
    GwySurface *surface, *duplicate;

    g_return_val_if_fail(GWY_IS_SURFACE(object), NULL);
    surface = GWY_SURFACE(object);
    duplicate = gwy_surface_new_alike(surface,FALSE);
    memcpy(duplicate->data, surface->data,
           3*surface->n*sizeof(gdouble));

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

    if (clone->n != surface->n)
        clone->data = (GwyXYZ*) g_renew(gdouble, clone->data, 3*surface->n);
    clone->n = surface->n;

    gwy_surface_copy(surface, clone, TRUE);
}

/**
 * gwy_surface_data_changed:
 * @surface: A data field.
 *
 * Emits signal "data-changed" on a surface.
 **/
void
gwy_surface_data_changed(GwySurface *surface)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_signal_emit(surface, surface_signals[DATA_CHANGED], 0);
}

void
gwy_surface_copy(GwySurface *src,
                 GwySurface *dest,
                 gboolean nondata_too)
{
    g_return_if_fail(GWY_IS_SURFACE(src));
    g_return_if_fail(GWY_IS_SURFACE(dest));
    g_return_if_fail(src->n == dest->n);

    memcpy(dest->data, src->data, src->n*3*sizeof(gdouble));

    dest->xmin  = src->xmin;
    dest->xmax  = src->xmax;
    dest->ymin  = src->ymin;
    dest->ymax  = src->ymax;

    dest->cached = src->cached;
    memcpy(dest->cache, src->cache, GWY_DATA_FIELD_CACHE_SIZE*sizeof(gdouble));

    if (!nondata_too)
        return;

    /* SI Units can be NULL */
    if (src->si_unit_xy && dest->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(src->si_unit_xy),
                               G_OBJECT(dest->si_unit_xy));
    else if (src->si_unit_xy && !dest->si_unit_xy)
        dest->si_unit_xy = gwy_si_unit_duplicate(src->si_unit_xy);
    else if (!src->si_unit_xy && dest->si_unit_xy)
        gwy_object_unref(dest->si_unit_xy);

    if (src->si_unit_z && dest->si_unit_z)
        gwy_serializable_clone(G_OBJECT(src->si_unit_z),
                               G_OBJECT(dest->si_unit_z));
    else if (src->si_unit_z && !dest->si_unit_z)
        dest->si_unit_z = gwy_si_unit_duplicate(src->si_unit_z);
    else if (!src->si_unit_z && dest->si_unit_z)
        gwy_object_unref(dest->si_unit_z);
}

static void
alloc_data(GwySurface *surface)
{
    g_free(surface->data);
    if (surface->n)
        surface->data = g_new(GwyXYZ, surface->n);
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
 **/
GwySurface*
gwy_surface_new_part(const GwySurface *surface,
                     gdouble xfrom,
                     gdouble xto,
                     gdouble yfrom,
                     gdouble yto)
{
    GwySurface *part;
    guint n = 0;
    guint i;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    part = gwy_surface_new_alike(surface, FALSE);

    if (!surface->n
        || xfrom > xto
        || yfrom > yto)
        return part;


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

void
copy_field_to_surface(const GwyDataField *field,
                      GwySurface *surface)
{
    gdouble dx = (field->xreal) / ((gdouble)(field->xres));
    gdouble dy = (field->yreal) / ((gdouble)(field->yres));
    gdouble xoff = 0.5*dx + field->xoff, yoff = 0.5*dy + field->yoff;
    guint k = 0;
    guint i,j;

    for (i = 0; i < field->yres; i++) {
        for (j = 0; j < field->xres; j++) {
            surface->data[k].x = dx*j + xoff;
            surface->data[k].y = dy*i + yoff;
            surface->data[k].z = field->data[k];
            k++;
        }
    }
    if (field->si_unit_xy)
        surface->si_unit_xy = gwy_si_unit_duplicate(field->si_unit_xy);
    if (field->si_unit_z)
        surface->si_unit_z = gwy_si_unit_duplicate(field->si_unit_z);

    gwy_surface_invalidate(surface);
    surface->cached_range = TRUE;
    surface->xmin = xoff;
    surface->xmax = dx*(field->xres - 1) + xoff;
    surface->ymin = yoff;
    surface->ymax = dy*(field->yres - 1) + yoff;
}

/**
 * gwy_surface_new_from_field:
 * @field: A one-dimensional data field.
 *
 * Creates a new surface from a one-dimensional data field.
 *
 * The number of points in the new surface will be equal to the number of
 * points in the field.  Lateral coordinates will be equal to the corresponding
 * @field coordinates; values will be created in regular grid according to
 * @field's physical size and offset.
 *
 * Lateral and value units will correspond to @field's units.
 *
 * Returns: A new surface.
 **/
GwySurface*
gwy_surface_new_from_field(const GwyDataField *datafield)
{
    GwySurface *surface;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(datafield), NULL);

    surface = g_object_newv(GWY_TYPE_SURFACE, 0, NULL);

    surface->n = datafield->xres * datafield->yres;
    alloc_data(surface);
    copy_field_to_surface(datafield, surface);

    return surface;
}


/**
 * gwy_surface_set_from_field:
 * @surface: A surface.
 * @field: A one-dimensional data field.
 *
 * Sets the data and units of a surface from a field.
 *
 * See gwy_surface_new_from_field() for details.
 **/
void
gwy_surface_set_from_field(GwySurface *surface,
                           const GwyDataField *field)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    g_return_if_fail(GWY_IS_DATA_FIELD(field));

    if (surface->n != field->xres * field->yres) {
        g_free(surface->data);
        surface->n = field->xres * field->yres;
        alloc_data(surface);

    }
    copy_field_to_surface(field, surface);
}

static gboolean
propagate_laplace(const gdouble *srcb, guint *cntrb,
                  gint xres, gint yres, gint j, gint i, guint iter,
                  gdouble *value)
{
    guint s = 0;
    gdouble z = 0.0;

    // Scan the pixel neighbourhood and gather already initialised values.
    if (i && j && cntrb[0] <= iter) {
        z += srcb[0];
        s++;
    }
    if (i && cntrb[1] <= iter) {
        z += srcb[1];
        s++;
    }
    if (i && j < (gint)xres-1 && cntrb[2] <= iter) {
        z += srcb[2];
        s++;
    }
    if (j && cntrb[xres] <= iter) {
        z += srcb[xres];
        s++;
    }
    if (j < (gint)xres-1 && cntrb[xres+2] <= iter) {
        z += srcb[xres+2];
        s++;
    }
    if (i < (gint)yres-1 && j && cntrb[2*xres] <= iter) {
        z += srcb[2*xres];
        s++;
    }
    if (i < (gint)yres-1 && cntrb[2*xres+1] <= iter) {
        z += srcb[2*xres+1];
        s++;
    }
    if (i < (gint)yres-1 && j < (gint)xres-1
        && cntrb[2*xres+2] <= iter) {
        z += srcb[2*xres+2];
        s++;
    }
    if (s) {
        *value = z/s;
        return TRUE;
    }
    return FALSE;
}

static void
regularise_preview(const GwySurface *surface,
                   GwyDataField *field)
{
    guint k, todo, iter;
    GwyDataField *buffer;

    guint xres = field->xres, yres = field->yres, totalcount = 0;
    gdouble dx = gwy_data_field_get_xmeasure(field), dy = gwy_data_field_get_ymeasure(field);
    guint *counters = g_new0(guint, xres*yres);

    gwy_data_field_clear(field);

    for (k = 0; k < surface->n; k++) {
        const GwyXYZ *pt = surface->data + k;
        gint j = (gint)floor((pt->x - field->xoff)/dx);
        gint i = (gint)floor((pt->y - field->yoff)/dy);
        if (j < 0 || j >= (gint)xres || i < 0 || i >= (gint)yres)
            continue;

        if (!counters[i*xres + j]++)
            totalcount++;
        field->data[i*xres + j] += pt->z;
    }

    if (!totalcount) {
        // FIXME: Do something clever instead.
        g_free(counters);
        return;
    }

    // If the pixels contain at leasr something use the mean value as the
    // representation and mark them as fixed.  Otherwise mark them as to be
    // interpolated in iter 1.
    for (k = 0; k < xres*yres; k++) {
        if (counters[k]) {
            field->data[k] /= counters[k];
            counters[k] = 0;
        }
        else
            counters[k] = G_MAXUINT;
    }

    todo = xres*yres - totalcount;
    if (!todo) {
        g_free(counters);
        return;
    }

    buffer = gwy_data_field_duplicate(field);

    for (iter = 0; todo; iter++) {
        guint i,j;
        // Interpolate in the already initialised area.
        for (i = 0; i < (gint)yres; i++) {
            for (j = 0; j < (gint)xres; j++) {
                gint l = i*xres-xres + j-1;
                guint *cb = counters + l;
                const gdouble *db = field->data + l;
                gdouble *bb = buffer->data + l;

                if (cb[xres+1] && cb[xres+1] <= iter)
                    propagate_laplace(db, cb, xres, yres, j, i, iter,
                                      bb + xres+1);
            }
        }
        GWY_SWAP(GwyDataField*, field, buffer);

        // Propagate already initialised values to the uninitialised area.
        for (i = 0; i < (gint)yres; i++) {
            for (j = 0; j < (gint)xres; j++) {
                gint l = i*xres-xres + j-1;
                guint *cb = counters + l;
                const gdouble *db = field->data + l;
                gdouble *bb = buffer->data + l;

                if (cb[xres+1] > iter) {
                    if (propagate_laplace(db, cb, xres, yres, j, i, iter,
                                          bb + xres+1)) {
                        // Mark it as done for the next iter but do not let the
                        // value propagate in this iter.
                        cb[xres+1] = iter+1;
                        todo--;
                    }
                }
            }
        }
        GWY_SWAP(GwyDataField*, field, buffer);
    }

    g_object_unref(buffer);
    g_free(counters);
}

static GwyDataField*
regularise(GwySurface *surface,
           GwySurfaceRegularizeType method,
           gboolean full,
           gdouble xfrom, gdouble xto,
           gdouble yfrom, gdouble yto,
           guint xres, guint yres)
{
    GwyDataField *field;
    gdouble xmin, xmax, ymin, ymax;
    gdouble xlen, ylen;

    gwy_surface_xrange_full(surface, &xmin, &xmax);
    gwy_surface_yrange_full(surface, &ymin, &ymax);

    if (full) {
        xfrom = xmin;
        xto = xmax;
        yfrom = ymin;
        yto = ymax;
    }

    xlen = xto - xfrom;
    ylen = yto - yfrom;

    if (!xres || !yres) {
        gdouble alpha = xlen/ylen, alpha1 = 1.0 - alpha;
        gdouble sqrtD = sqrt(4.0*alpha*surface->n + alpha1*alpha1);
        gdouble xresfull = 0.5*(alpha1 + sqrtD),
                yresfull = 0.5*(sqrtD - alpha1)/alpha;
        gdouble p = sqrt(xlen*ylen/(xresfull - 1)/(yresfull - 1));

        if (!xres) {
            if (!p || isnan(p) || !xlen)
                xres = 1;
            else {
                xres = GWY_ROUND(xlen/p + 1);
                xres = CLAMP(xres, 1, surface->n);
            }
        }
        if (!yres) {
            if (!p || isnan(p) || !ylen)
                yres = 1;
            else {
                yres = GWY_ROUND(ylen/p + 1);
                yres = CLAMP(yres, 1, surface->n);
            }
        }
    }

    field = gwy_data_field_new(xres, yres, 0.0, 0.0, FALSE);

    if (xres == 1 || !(xlen))
        field->xreal = xfrom ? fabs(xfrom) : 1.0;
    else
        field->xreal = (xlen)*xres/(xres - 1.0);
    field->xoff = xfrom - 0.5*gwy_data_field_get_xmeasure(field);

    if (yres == 1 || !(ylen))
        field->yreal = yfrom ? fabs(yfrom) : 1.0;
    else
        field->yreal = (ylen)*yres/(yres - 1.0);
    field->yoff = yfrom - 0.5*gwy_data_field_get_ymeasure(field);

    if (method == GWY_SURFACE_REGULARIZE_PREVIEW)
        regularise_preview(surface, field);
    else
        g_assert_not_reached();

    if (surface->si_unit_xy)
        field->si_unit_xy = gwy_si_unit_duplicate(surface->si_unit_xy);
    if (surface->si_unit_z)
        field->si_unit_z = gwy_si_unit_duplicate(surface->si_unit_z);

    return field;
}

/**
 * gwy_surface_regularize_full:
 * @surface: A surface.
 * @method: Regularisation method.
 * @xres: Required horizontal resolution.  Pass 0 to choose a resolution
 *        automatically.
 * @yres: Required vertical resolution.  Pass 0 to choose a resolution
 *        automatically.
 *
 * Creates a two-dimensional data field from an entire surface.
 *
 * If the surface is non-degenerate of size 2×2, composed of squares and
 * the requested @xres×@yres matches the @surfaces's number of points, then
 * one-to-one data point mapping can be used and the conversion will be
 * information-preserving.  In other words, if the surface was created from a
 * #GwyField this function can perform a perfect reversal, possibly up to some
 * rounding errors. This is true for any @method although the method choice
 * still can has a dramatic effect on speed and resource consumption.
 * Otherwise the interpolated and exterpolated values are method-dependent.
 *
 * Returns: (allow-none):
 *          A new two-dimensional data field or %NULL if the surface contains
 *          no points.
 **/
GwyDataField*
gwy_surface_regularize_full(GwySurface *surface,
                            GwySurfaceRegularizeType method,
                            guint xres, guint yres)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);

    if (!surface->n)
        return NULL;

    return regularise(surface, method, TRUE, 0.0, 0.0, 0.0, 0.0, xres, yres);
}



/**
 * gwy_surface_regularize:
 * @surface: A surface.
 * @method: Regularisation method.
 * @xfrom: Start the horizontal interval.
 * @xto: End of the horizontal interval.
 * @yfrom: Start the vertical interval.
 * @yto: End of the vertical interval.
 * @xres: Required horizontal resolution.  Pass 0 to choose a resolution
 *        automatically.
 * @yres: Required vertical resolution.  Pass 0 to choose a resolution
 *        automatically.
 *
 * Creates a two-dimensional data field from a surface.
 *
 * Returns: (allow-none):
 *          A new two-dimensional data field or %NULL if the surface contains
 *          no points.
 **/
GwyDataField*
gwy_surface_regularize(GwySurface *surface,
                       GwySurfaceRegularizeType method,
                       gdouble xfrom, gdouble xto,
                       gdouble yfrom, gdouble yto,
                       guint xres, guint yres)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    g_return_val_if_fail(xto >= xfrom, NULL);
    g_return_val_if_fail(yto >= yfrom, NULL);

    if (!surface->n)
        return NULL;

    return regularise(surface, method, FALSE,
                      xfrom, xto, yfrom, yto, xres, yres);
}

/**
 * gwy_surface_format_xy:
 * @surface: A surface.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying lateral values of a surface.
 *
 * The created format usually has a sufficient precision to represent lateral
 * values of neighbour points as different values.  However, if the intervals
 * differ by several orders of magnitude this is not really guaranteed.
 *
 * Returns: A newly created value format.
 **/
GwySIValueFormat*
gwy_surface_get_value_format_xy(GwySurface *surface,
                                GwySIUnitFormatStyle style,
                                GwySIValueFormat *format)
{
    gdouble xmin, xmax, ymin, ymax, max, unit;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    if (!surface->n)
        return gwy_si_unit_get_format_with_resolution(surface->si_unit_xy,
                                               style, 1.0, 0.1,format);
    // XXX: If we have the triangulation a better estimate can be made.  Maybe.
    if (surface->n == 1) {
        gdouble m = MAX(fabs(surface->data[0].x), fabs(surface->data[0].y));
        if (!m)
            m = 1.0;
        return gwy_si_unit_get_format_with_resolution(surface->si_unit_xy,
                                                      style, m, m/10.0,format);
    }
    gwy_surface_xrange_full(surface, &xmin, &xmax);
    gwy_surface_yrange_full(surface, &ymin, &ymax);
    max = MAX(MAX(fabs(xmax), fabs(xmin)), MAX(fabs(ymax), fabs(ymin)));
    if (!max)
        max = 1.0;
    unit = hypot(ymax - ymin, xmax - xmin)/sqrt(surface->n);
    if (!unit)
        unit = max/10.0;
    return gwy_si_unit_get_format_with_resolution(surface->si_unit_xy,
                                           style, max, 0.3*unit,format);
}

/**
 * gwy_surface_format_z:
 * @surface: A surface.
 * @style: Value format style.
 *
 * Finds a suitable format for displaying values in a data surface.
 *
 * Returns: A newly created value format.
 **/
GwySIValueFormat*
gwy_surface_get_format_z(GwySurface *surface,
                     GwySIUnitFormatStyle style,
                     GwySIValueFormat *format)
{
    gdouble min, max;
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NULL);
    if (surface->n) {
        gwy_surface_min_max_full(surface, &min, &max);
        if (max == min) {
            max = ABS(max);
            min = 0.0;
        }
    }
    else {
        min = 0.0;
        max = 1.0;
    }
    return gwy_si_unit_get_format_with_digits(surface->si_unit_z,
                                       style, max - min, 3,format);
}

/*--------TESTING PURPOSES ------------*/

void
gwy_surface_print_info(GwySurface *surface, int n_values)
{
    int i;
    GwyXYZ* p;

    printf("xmin: %f\txmax: %f\tymin: %f\tymax: %f\tpoints:%d\n",
           surface->xmin,
           surface->xmax,
           surface->ymin,
           surface->ymax,
           surface->n
           );

    for(i=0,p=surface->data;i<n_values && i<surface->n;i++,p++)
    {
      printf("%f \t%f \t%f\n",p->x,p->y,p->z);
    }

}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
