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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/interpolation.h>
#include <libprocess/gwycaldata.h>

#include <libprocess/natural.h>

#define GWY_CALDATA_TYPE_NAME "GwyCalData"



struct _GwyCalData {
    GObject parent_instance;

    gdouble *x;
    gdouble *y;
    gdouble *z;
    gdouble *xerr;
    gdouble *yerr;
    gdouble *zerr;
    gdouble *xunc;
    gdouble *yunc;
    gdouble *zunc;
    gdouble x_from;
    gdouble x_to;
    gdouble y_from;
    gdouble y_to;
    gdouble z_from;
    gdouble z_to;
    gint ndata;

    GwySIUnit *si_unit_x;
    GwySIUnit *si_unit_y;
    GwySIUnit *si_unit_z;

    GwyDelaunayVertex *err_ps;      //as all triangulation works for vectors, there are two separate meshes now which is stupid.
    GwyDelaunayVertex *unc_ps;
    GwyDelaunayMesh *err_m;
    GwyDelaunayMesh *unc_m;

    gpointer reserved1;
    gint int1;
};

struct _GwyCalDataClass {
    GObjectClass parent_class;

    void (*reserved1)(void);
};


enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void        gwy_caldata_finalize         (GObject *object);
static void        gwy_caldata_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_caldata_serialize        (GObject *obj,
                                                   GByteArray *buffer);
static gsize       gwy_caldata_get_size         (GObject *obj);
static GObject*    gwy_caldata_deserialize      (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);

/*static GObject*    gwy_caldata_duplicate_real   (GObject *object);
static void        gwy_caldata_clone_real       (GObject *source,
                                                   GObject *copy);
*/

G_DEFINE_TYPE_EXTENDED
    (GwyCalData, gwy_caldata, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_caldata_serializable_init))

static void
gwy_caldata_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_caldata_serialize;
    iface->deserialize = gwy_caldata_deserialize;
    iface->get_size = gwy_caldata_get_size;
    /*iface->duplicate = gwy_caldata_duplicate_real;
    iface->clone = gwy_caldata_clone_real;*/
}

static void
gwy_caldata_class_init(GwyCalDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_caldata_finalize;
}

static void
gwy_caldata_init(GwyCalData *caldata)
{
    gwy_debug_objects_creation(G_OBJECT(caldata));
}

static void
gwy_caldata_finalize(GObject *object)
{
    GwyCalData *caldata = (GwyCalData*)object;

    gwy_object_unref(caldata->si_unit_x);
    gwy_object_unref(caldata->si_unit_y);
    gwy_object_unref(caldata->si_unit_z);

    _gwy_delaunay_mesh_free(caldata->err_m);
    _gwy_delaunay_mesh_free(caldata->unc_m);
    //_gwy_delaunay_vertex_free(caldata->err_m);
    //_gwy_delaunay_vertex_free(caldata->unc_m);

    g_free(caldata->x);
    g_free(caldata->y);
    g_free(caldata->z);
    g_free(caldata->xerr);
    g_free(caldata->yerr);
    g_free(caldata->zerr);
    g_free(caldata->xunc);
    g_free(caldata->yunc);
    g_free(caldata->zunc);

    G_OBJECT_CLASS(gwy_caldata_parent_class)->finalize(object);
}

/**
 * gwy_caldata_new:
 * @ndata: Number of calibration data
 *
 * Creates new calibration data.
 *
 * Returns: A newly created calibration data.
 **/
GwyCalData*
gwy_caldata_new(gint ndata)
{
    GwyCalData *caldata;

    gwy_debug("");
    caldata = g_object_new(GWY_TYPE_CALDATA, NULL);

    caldata->ndata = ndata;
    caldata->x = g_new(gdouble, caldata->ndata);
    caldata->y = g_new(gdouble, caldata->ndata);
    caldata->z = g_new(gdouble, caldata->ndata);
    caldata->xerr = g_new(gdouble, caldata->ndata);
    caldata->yerr = g_new(gdouble, caldata->ndata);
    caldata->zerr = g_new(gdouble, caldata->ndata);
    caldata->xunc = g_new(gdouble, caldata->ndata);
    caldata->yunc = g_new(gdouble, caldata->ndata);
    caldata->zunc = g_new(gdouble, caldata->ndata);

    return caldata;
}

/**
 * gwy_caldata_resize
 * @caldata: Calibration data
 * @ndata: New number of data points
 *
 * Set number of calibration data entries, resize
 * arrays for holding them. Preserves actual values
 * up to new calibration data size.
 *
 **/

void        
gwy_caldata_resize(GwyCalData *caldata, gint ndata)
{
    gint i, ncopy;
    gdouble *b;

    if (ndata==caldata->ndata) return;

    ncopy = MIN(caldata->ndata, ndata);
    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->x[i];
    }
    caldata->x = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->y[i];
    }
    caldata->y = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->z[i];
    }
    caldata->z = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->xerr[i];
    }
    caldata->xerr = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->yerr[i];
    }
    caldata->yerr = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->zerr[i];
    }
    caldata->zerr = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->xunc[i];
    }
    caldata->xunc = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->yunc[i];
    }
    caldata->yunc = b;

    b = g_new(gdouble, ndata);
    for (i=0; i<ncopy; i++) {
        b[i] = caldata->zunc[i];
    }
    caldata->zunc = b;

}

/**
 * gwy_caldata_append
 * @caldata: Calibration data
 * @sec: Calibration data to be appended
 *
 * Append calibration data entries, resizing
 * arrays for holding them. 
 *
 **/

void        
gwy_caldata_append(GwyCalData *caldata, GwyCalData *sec)
{
    gint i, from;
 
    from = caldata->ndata;
    gwy_caldata_resize(caldata, caldata->ndata + sec->ndata);

    for (i=from; i<(caldata->ndata + sec->ndata); i++)
    {
        caldata->x[i] = sec->x[i-from];
        caldata->y[i] = sec->y[i-from];
        caldata->z[i] = sec->z[i-from];
        caldata->xerr[i] = sec->xerr[i-from];
        caldata->yerr[i] = sec->yerr[i-from];
        caldata->zerr[i] = sec->zerr[i-from];
        caldata->xunc[i] = sec->xunc[i-from];
        caldata->yunc[i] = sec->yunc[i-from];
        caldata->zunc[i] = sec->zunc[i-from];
    }
}

/**
 * gwy_caldata_get_ndata:
 * @caldata: Calibration data
 *
 * Get number of calibration data entries.
 *
 * Returns: Number of calibration data entries.
 **/
gint
gwy_caldata_get_ndata(GwyCalData *caldata)
{
    return caldata->ndata;
}

static GByteArray*
gwy_caldata_serialize(GObject *obj,
                        GByteArray *buffer)
{
    GwyCalData *caldata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CALDATA(obj), NULL);

    caldata = GWY_CALDATA(obj);
    if (!caldata->si_unit_x)
        caldata->si_unit_x = gwy_si_unit_new("");
    if (!caldata->si_unit_y)
        caldata->si_unit_y = gwy_si_unit_new("");
    if (!caldata->si_unit_z)
        caldata->si_unit_z = gwy_si_unit_new("");
    {
        GwySerializeSpec spec[] = {
            { 'i', "ndata", &caldata->ndata, NULL, },
            { 'o', "si_unit_x", &caldata->si_unit_x, NULL, },
            { 'o', "si_unit_y", &caldata->si_unit_y, NULL, },
            { 'o', "si_unit_z", &caldata->si_unit_z, NULL, },
            { 'd', "x_from", &caldata->x_from, NULL, },
            { 'd', "y_from", &caldata->y_from, NULL, },
            { 'd', "z_from", &caldata->z_from, NULL, },
            { 'd', "x_to", &caldata->x_to, NULL, },
            { 'd', "y_to", &caldata->y_to, NULL, },
            { 'd', "z_to", &caldata->z_to, NULL, },
            { 'D', "x", &caldata->x, &caldata->ndata, },
            { 'D', "y", &caldata->y, &caldata->ndata, },
            { 'D', "z", &caldata->z, &caldata->ndata, },
            { 'D', "xerr", &caldata->xerr, &caldata->ndata, },
            { 'D', "yerr", &caldata->yerr, &caldata->ndata, },
            { 'D', "zerr", &caldata->zerr, &caldata->ndata, },
            { 'D', "xunc", &caldata->xunc, &caldata->ndata, },
            { 'D', "yunc", &caldata->yunc, &caldata->ndata, },
            { 'D', "zunc", &caldata->zunc, &caldata->ndata, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_CALDATA_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_caldata_get_size(GObject *obj)
{
    GwyCalData *caldata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CALDATA(obj), 0);

    caldata = GWY_CALDATA(obj);
    if (!caldata->si_unit_x)
        caldata->si_unit_x = gwy_si_unit_new("");
    if (!caldata->si_unit_y)
        caldata->si_unit_y = gwy_si_unit_new("");
    if (!caldata->si_unit_z)
        caldata->si_unit_z = gwy_si_unit_new("");
    {
        GwySerializeSpec spec[] = {
            { 'i', "ndata", &caldata->ndata, NULL, },
            { 'o', "si_unit_x", &caldata->si_unit_x, NULL, },
            { 'o', "si_unit_y", &caldata->si_unit_y, NULL, },
            { 'o', "si_unit_z", &caldata->si_unit_z, NULL, },
            { 'd', "x_from", &caldata->x_from, NULL, },
            { 'd', "y_from", &caldata->y_from, NULL, },
            { 'd', "z_from", &caldata->z_from, NULL, },
            { 'd', "x_to", &caldata->x_to, NULL, },
            { 'd', "y_to", &caldata->y_to, NULL, },
            { 'd', "z_to", &caldata->z_to, NULL, },
            { 'D', "x", &caldata->x, &caldata->ndata, },
            { 'D', "y", &caldata->y, &caldata->ndata, },
            { 'D', "z", &caldata->z, &caldata->ndata, },
            { 'D', "xerr", &caldata->xerr, &caldata->ndata, },
            { 'D', "yerr", &caldata->yerr, &caldata->ndata, },
            { 'D', "zerr", &caldata->zerr, &caldata->ndata, },
            { 'D', "xunc", &caldata->xunc, &caldata->ndata, },
            { 'D', "yunc", &caldata->yunc, &caldata->ndata, },
            { 'D', "zunc", &caldata->zunc, &caldata->ndata, },
        };

        return gwy_serialize_get_struct_size(GWY_CALDATA_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_caldata_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guint32 fsize;
    gint ndata;
    gdouble x_from, y_from, z_from, x_to, y_to, z_to;
    gdouble *x = NULL, *y = NULL, *z = NULL;
    gdouble *xerr = NULL, *yerr = NULL, *zerr = NULL;
    gdouble *xunc = NULL, *yunc = NULL, *zunc = NULL;
    GwySIUnit *si_unit_x = NULL, *si_unit_y = NULL, *si_unit_z = NULL;
    GwyCalData *caldata;

    GwySerializeSpec spec[] = {
      { 'i', "ndata", &ndata, NULL, },
      { 'o', "si_unit_x", &si_unit_x, NULL, },
      { 'o', "si_unit_y", &si_unit_y, NULL, },
      { 'o', "si_unit_z", &si_unit_z, NULL, },
      { 'd', "x_from", &x_from, NULL, },
      { 'd', "y_from", &y_from, NULL, },
      { 'd', "z_from", &z_from, NULL, },
      { 'd', "x_to", &x_to, NULL, },
      { 'd', "y_to", &y_to, NULL, },
      { 'd', "z_to", &z_to, NULL, },
      { 'D', "x", &x, &fsize, },
      { 'D', "y", &y, &fsize, },
      { 'D', "z", &z, &fsize, },
      { 'D', "xerr", &xerr, &fsize, },
      { 'D', "yerr", &yerr, &fsize, },
      { 'D', "zerr", &zerr, &fsize, },
      { 'D', "xunc", &xunc, &fsize, },
      { 'D', "yunc", &yunc, &fsize, },
      { 'D', "zunc", &zunc, &fsize, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);


    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_CALDATA_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(x);
        g_free(y);
        g_free(z);
        g_free(xerr);
        g_free(yerr);
        g_free(zerr);
        g_free(xunc);
        g_free(yunc);
        g_free(zunc);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        gwy_object_unref(si_unit_z);
        return NULL;
    }
    if (fsize != (guint)ndata) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_CALDATA_TYPE_NAME, fsize, ndata);

        g_free(x);
        g_free(y);
        g_free(z);
        g_free(xerr);
        g_free(yerr);
        g_free(zerr);
        g_free(xunc);
        g_free(yunc);
        g_free(zunc);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        gwy_object_unref(si_unit_z);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    caldata = gwy_caldata_new(1);
    g_free(caldata->x);
    g_free(caldata->y);
    g_free(caldata->z);
    g_free(caldata->xerr);
    g_free(caldata->yerr);
    g_free(caldata->zerr);
    g_free(caldata->xunc);
    g_free(caldata->yunc);
    g_free(caldata->zunc);
    caldata->ndata = ndata;
    caldata->x = x;
    caldata->y = y;
    caldata->z = z;
    caldata->x_from = x_from;
    caldata->y_from = y_from;
    caldata->z_from = z_from;
    caldata->x_to = x_to;
    caldata->y_to = y_to;
    caldata->z_to = z_to;
    caldata->xerr = xerr;
    caldata->yerr = yerr;
    caldata->zerr = zerr;
    caldata->xunc = xunc;
    caldata->yunc = yunc;
    caldata->zunc = zunc;
    if (si_unit_x) {
        gwy_object_unref(caldata->si_unit_x);
        caldata->si_unit_x = si_unit_x;
    }
    if (si_unit_y) {
        gwy_object_unref(caldata->si_unit_y);
        caldata->si_unit_y = si_unit_y;
    }
    if (si_unit_z) {
        gwy_object_unref(caldata->si_unit_z);
        caldata->si_unit_z = si_unit_z;
    }
    return (GObject*)caldata;
}

/**
 * gwy_caldata_get_si_unit_x:
 * @caldata: Calibration data
 *
 * Returns lateral SI unit of calibration data.
 *
 * Returns: SI unit corresponding to the lateral (X) dimension of the calibration data
 *          Its reference count is not incremented.
 **/
GwySIUnit*
gwy_caldata_get_si_unit_x(GwyCalData *caldata)
{
    g_return_val_if_fail(GWY_IS_CALDATA(caldata), NULL);

    if (!caldata->si_unit_x)
        caldata->si_unit_x = gwy_si_unit_new("");

    return caldata->si_unit_x;
}

/**
 * gwy_caldata_get_si_unit_y:
 * @caldata: Calibration data.
 *
 * Returns lateral SI unit of calibration data
 *
 * Returns: SI unit corresponding to the lateral (Y) dimension of the calibration data.
 *          Its reference count is not incremented.
 **/
GwySIUnit*
gwy_caldata_get_si_unit_y(GwyCalData *caldata)
{
    g_return_val_if_fail(GWY_IS_CALDATA(caldata), NULL);

    if (!caldata->si_unit_y)
        caldata->si_unit_y = gwy_si_unit_new("");

    return caldata->si_unit_y;
}


/**
 * gwy_caldata_get_si_unit_z:
 * @caldata: Calibration data.
 *
 * Returns value SI unit of calibration data
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of calibration data.
 *          Its reference count is not incremented.
 **/
GwySIUnit*
gwy_caldata_get_si_unit_z(GwyCalData *caldata)
{
    g_return_val_if_fail(GWY_IS_CALDATA(caldata), NULL);

    if (!caldata->si_unit_z)
        caldata->si_unit_z = gwy_si_unit_new("");

    return caldata->si_unit_z;
}

/**
 * gwy_caldata_set_si_unit_x:
 * @caldata: Calibration data.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (X) dimension of
 * calibration data.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_caldata_set_si_unit_x(GwyCalData *caldata,
                            GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_CALDATA(caldata));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (caldata->si_unit_x == si_unit)
        return;

    gwy_object_unref(caldata->si_unit_x);
    g_object_ref(si_unit);
    caldata->si_unit_x = si_unit;
}

/**
 * gwy_caldata_set_si_unit_y:
 * @caldata: Calibration data.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (Y) dimension of
 * calibration data.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_caldata_set_si_unit_y(GwyCalData *caldata,
                            GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_CALDATA(caldata));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (caldata->si_unit_y == si_unit)
        return;

    gwy_object_unref(caldata->si_unit_y);
    g_object_ref(si_unit);
    caldata->si_unit_y = si_unit;
}

/**
 * gwy_caldata_set_si_unit_z:
 * @caldata: Calibration data.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of
 * calibration data.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_caldata_set_si_unit_z(GwyCalData *caldata,
                            GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_CALDATA(caldata));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (caldata->si_unit_z == si_unit)
        return;

    gwy_object_unref(caldata->si_unit_z);
    g_object_ref(si_unit);
    caldata->si_unit_z = si_unit;
}

/**
 * gwy_caldata_setup_interpolation:
 * @caldata: Calibration data.
 *
 * Prepares data for interpolating the calibration data (building Delaunay triangulation, etc.).
 **/
void
gwy_caldata_setup_interpolation (GwyCalData *caldata)
{
    caldata->err_ps = _gwy_delaunay_vertex_new(caldata->x, caldata->y, caldata->z,
                                 caldata->xerr, caldata->yerr, caldata->zerr, caldata->ndata);
    caldata->unc_ps = _gwy_delaunay_vertex_new(caldata->x, caldata->y, caldata->z,
                                 caldata->xunc, caldata->yunc, caldata->zunc, caldata->ndata);

    caldata->err_m = _gwy_delaunay_mesh_new();
    caldata->unc_m = _gwy_delaunay_mesh_new();

    _gwy_delaunay_mesh_build(caldata->err_m, caldata->err_ps, caldata->ndata);
    _gwy_delaunay_mesh_build(caldata->unc_m, caldata->unc_ps, caldata->ndata);
}

/**
 * gwy_caldata_interpolate:
 * @caldata: Calibration data.
 * @x: x coordinate of requested position
 * @y: y coordinate of requested position
 * @z: z coordinate of requested position
 * @xerr: x error at given position
 * @yerr: y error at given position
 * @zerr: z error at given position
 * @xunc: x uncertainty at given position
 * @yunc: y uncertainty at given position
 * @zunc: z uncertainty at given position
 *
 * Determines (interpolates) caldata parameters for given position.
 **/
void
gwy_caldata_interpolate(GwyCalData *caldata,
                        gdouble x, gdouble y, gdouble z,
                        gdouble *xerr, gdouble *yerr, gdouble *zerr,
                        gdouble *xunc, gdouble *yunc, gdouble *zunc)
{

    if (xerr || yerr || zerr)
       _gwy_delaunay_mesh_interpolate3_3(caldata->err_m, x, y, z, xerr, yerr, zerr);

    if (xunc || yunc || zunc)
       _gwy_delaunay_mesh_interpolate3_3(caldata->unc_m, x, y, z, xunc, yunc, zunc);
}

/**
 * gwy_caldata_get_xerr:
 * @caldata: Calibration data.
 *
 * Returns: x error array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_xerr(GwyCalData *caldata)
{
    return caldata->xerr;
}
/**
 * gwy_caldata_get_x:
 * @caldata: Calibration data.
 *
 * Returns: x array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_x(GwyCalData *caldata)
{
    return caldata->x;
}
/**
 * gwy_caldata_get_y:
 * @caldata: Calibration data.
 *
 * Returns: y array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_y(GwyCalData *caldata)
{
    return caldata->y;
}
/**
 * gwy_caldata_get_z:
 * @caldata: Calibration data.
 *
 * Returns: z array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_z(GwyCalData *caldata)
{
    return caldata->z;
}
/**
 * gwy_caldata_get_yerr:
 * @caldata: Calibration data.
 *
 * Returns: y error array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_yerr(GwyCalData *caldata)
{
    return caldata->yerr;
}
/**
 * gwy_caldata_get_zerr:
 * @caldata: Calibration data.
 *
 * Returns: z error array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_zerr(GwyCalData *caldata)
{
    return caldata->zerr;
}
/**
 * gwy_caldata_get_xunc:
 * @caldata: Calibration data.
 *
 * Returns: x uncertainty array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_xunc(GwyCalData *caldata)
{
    return caldata->xunc;
}
/**
 * gwy_caldata_get_yunc:
 * @caldata: Calibration data.
 *
 * Returns: y uncertainty array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_yunc(GwyCalData *caldata)
{
    return caldata->yunc;
}
/**
 * gwy_caldata_get_zunc:
 * @caldata: Calibration data.
 *
 * Returns: z uncertainty array pointer for given calibration data.
 **/
gdouble*    
gwy_caldata_get_zunc(GwyCalData *caldata)
{
    return caldata->zunc;
}


/**
 * gwy_caldata_get_range:
 * @caldata: Calibration data.
 * @xfrom: x minimum 
 * @xto: x maximum 
 * @yfrom: y minimum 
 * @yto: y maximum 
 * @zfrom: z minimum 
 * @zto: z maximum
 *
 * Returns boundaries of calibration data validity.
 **/
void        
gwy_caldata_get_range(GwyCalData *caldata,
                     gdouble *xfrom, gdouble *xto, gdouble *yfrom, gdouble *yto,
                     gdouble *zfrom, gdouble *zto)
{
    *xfrom = caldata->x_from;
    *xto = caldata->x_to;
    *yfrom = caldata->y_from;
    *yto = caldata->y_to;
    *zfrom = caldata->z_from;
    *zto = caldata->z_to;
}

/**
 * gwy_caldata_get_range:
 * @caldata: Calibration data.
 * @xfrom: x minimum 
 * @xto: x maximum 
 * @yfrom: y minimum 
 * @yto: y maximum 
 * @zfrom: z minimum 
 * @zto: z maximum
 *
 * Sets boundaries of calibration data validity.
 **/
void        
gwy_caldata_set_range(GwyCalData *caldata,
                     gdouble xfrom, gdouble xto, gdouble yfrom, gdouble yto,
                     gdouble zfrom, gdouble zto)
{
    caldata->x_from = xfrom;
    caldata->x_to = xto;
    caldata->y_from = yfrom;
    caldata->y_to = yto;
    caldata->z_from = zfrom;
    caldata->z_to = zto;
}

void        
gwy_caldata_save_data(GwyCalData *caldata, gchar *filename)
{
    GByteArray *barray;

    if (!g_file_test(g_build_filename(gwy_get_user_dir(), "caldata", NULL), G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "caldata", NULL), 0700);
    }

    barray = gwy_serializable_serialize(G_OBJECT(caldata), NULL);
    if (!g_file_set_contents(filename, barray->data, sizeof(guint8)*barray->len, NULL))
    {
        g_warning("Cannot save caldata\n");
    }
}


/*
#include <stdio.h>
void
gwy_caldata_debug(GwyCalData *caldata, gchar *message)
{
    gint i;
    printf("%s: %d calibration data entries\n", message, caldata->ndata);

    for (i=0; i<caldata->ndata; i++)
    {
         printf("%d   (%g %g %g)  (%g %g %g)  (%g %g %g)\n", i,
                caldata->x[i], caldata->y[i], caldata->z[i],
                caldata->xerr[i], caldata->yerr[i], caldata->zerr[i],
                caldata->xunc[i], caldata->yunc[i], caldata->zunc[i]);
    }
}
*/

/************************** Documentation ****************************/

/**
 * SECTION:gwycaldata
 * @title: GwyCalData
 * @short_description: General calibration data
 *
 * #GwyCalData is an object representing general calibration data of a SPM
 * system.  Any point in the volume that can be reached by SPM scanner can be
 * characterized by two vectors: error and uncertainty. Errors can be used for
 * further data correction, uncertainties for propagation and determination of
 * final uncertainty of results of direct measurements or statistical
 * functions. Using different strategies a different number of these local
 * calibration data can be obtained, starting from single uncertainty applied
 * for whole system up to complex determination of local  SPM errors and
 * uncertainties.
 **/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
