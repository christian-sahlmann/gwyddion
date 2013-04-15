/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Originally based on Yeti's implementation for Gwyddion 3 branch,
 *  backported and modified for test use in Gwyddion 2 branch.
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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/brick.h>
#include <libprocess/interpolation.h>
#include <stdlib.h>

#define GWY_BRICK_TYPE_NAME "GwyBrick"


enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void        gwy_brick_finalize         (GObject *object);
static void        gwy_brick_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_brick_serialize        (GObject *obj,
                                               GByteArray *buffer);
static gsize       gwy_brick_get_size         (GObject *obj);
static GObject*    gwy_brick_deserialize      (const guchar *buffer,
                                               gsize size,
                                               gsize *position);
static GObject*    gwy_brick_duplicate_real   (GObject *object);
static void        gwy_brick_clone_real       (GObject *source,
                                               GObject *copy);

static guint brick_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyBrick, gwy_brick, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_brick_serializable_init))

static void
gwy_brick_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_brick_serialize;
    iface->deserialize = gwy_brick_deserialize;
    iface->get_size = gwy_brick_get_size;
    iface->duplicate = gwy_brick_duplicate_real;
    iface->clone = gwy_brick_clone_real;
}

static void
gwy_brick_class_init(GwyBrickClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_brick_finalize;

    /**
     * GwyBrick::data-changed:
     * @gwydataline: The #GwyBrick which received the signal.
     *
     * The ::data-changed signal is never emitted by data line itself.  It
     * is intended as a means to notify others data line users they should
     * update themselves.
     */
    brick_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyBrickClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_brick_init(GwyBrick *brick)
{
    gwy_debug_objects_creation(G_OBJECT(brick));
}

static void
gwy_brick_finalize(GObject *object)
{
    GwyBrick *brick = (GwyBrick*)object;

    gwy_object_unref(brick->si_unit_x);
    gwy_object_unref(brick->si_unit_y);
    g_free(brick->data);

    G_OBJECT_CLASS(gwy_brick_parent_class)->finalize(object);
}

/**
 * gwy_brick_new:
 * @xres: X resolution, i.e., the number of samples in x direction
 * @xres: Y resolution, i.e., the number of samples in y direction
 * @xres: Z resolution, i.e., the number of samples in z direction
 * @xreal: Real physical dimension in x direction.
 * @yreal: Real physical dimension in y direction.
 * @zreal: Real physical dimension in z direction.
 * @nullme: Whether the data brick should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data brick.
 *
 * Returns: A newly created data brick.
 *
 * Since: 2.31
 **/
GwyBrick*
gwy_brick_new(gint xres, gint yres, gint zres, gdouble xreal, gdouble yreal, gdouble zreal, gboolean nullme)
{
    GwyBrick *brick;

    gwy_debug("");
    brick = g_object_new(GWY_TYPE_BRICK, NULL);

    brick->xres = xres;
    brick->yres = yres;
    brick->zres = zres;
    brick->xreal = xreal;
    brick->yreal = yreal;
    brick->zreal = zreal;

    if (nullme)
        brick->data = g_new0(gdouble, brick->xres * brick->yres * brick->zres);
    else
        brick->data = g_new(gdouble, brick->xres * brick->yres * brick->zres);

    return brick;
}

/**
 * gwy_brick_new_alike:
 * @model: A data brick to take resolutions and units from.
 * @nullme: Whether the data brick should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data brick similar to an existing one.
 *
 * Use gwy_brick_duplicate() if you want to copy a data brick including
 * data.
 *
 * Returns: A newly created data brick.
 *
 * Since: 2.31
 **/
GwyBrick*
gwy_brick_new_alike(GwyBrick *model,
                    gboolean nullme)
{
    GwyBrick *brick;

    g_return_val_if_fail(GWY_IS_BRICK(model), NULL);
    brick = g_object_new(GWY_TYPE_BRICK, NULL);

    brick->xres = model->xres;
    brick->yres = model->yres;
    brick->zres = model->zres;
    brick->xreal = model->xreal;
    brick->yreal = model->yreal;
    brick->zreal = model->zreal;
    brick->xoff = model->xoff;
    brick->yoff = model->yoff;
    brick->zoff = model->zoff;
    if (nullme)
        brick->data = g_new0(gdouble, brick->xres * brick->yres * brick->zres);
    else
        brick->data = g_new(gdouble, brick->xres * brick->yres * brick->zres);

    if (model->si_unit_x)
        brick->si_unit_x = gwy_si_unit_duplicate(model->si_unit_x);
    if (model->si_unit_y)
        brick->si_unit_y = gwy_si_unit_duplicate(model->si_unit_y);
    if (model->si_unit_z)
        brick->si_unit_z = gwy_si_unit_duplicate(model->si_unit_z);
    if (model->si_unit_w)
        brick->si_unit_w = gwy_si_unit_duplicate(model->si_unit_w);

    return brick;
}


static GByteArray*
gwy_brick_serialize(GObject *obj,
                    GByteArray *buffer)
{
    GwyBrick *brick;
    guint32 datasize;
    gdouble *pxoff, *pyoff, *pzoff;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_BRICK(obj), NULL);

    brick = GWY_BRICK(obj);
    if (!brick->si_unit_x)
        brick->si_unit_x = gwy_si_unit_new("");
    if (!brick->si_unit_y)
        brick->si_unit_y = gwy_si_unit_new("");
    if (!brick->si_unit_z)
        brick->si_unit_z = gwy_si_unit_new("");
    if (!brick->si_unit_w)
        brick->si_unit_w = gwy_si_unit_new("");
    pxoff = brick->xoff ? &brick->xoff : NULL;
    pyoff = brick->yoff ? &brick->yoff : NULL;
    pzoff = brick->zoff ? &brick->zoff : NULL;

    datasize = brick->xres * brick->yres * brick->zres;

    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &brick->xres, NULL, },
            { 'i', "yres", &brick->yres, NULL, },
            { 'i', "zres", &brick->zres, NULL, },
            { 'd', "xreal", &brick->xreal, NULL, },
            { 'd', "yreal", &brick->yreal, NULL, },
            { 'd', "zreal", &brick->zreal, NULL, },
            { 'd', "xoff", pxoff, NULL, },
            { 'd', "yoff", pyoff, NULL, },
            { 'd', "zoff", pzoff, NULL, },
            { 'o', "si_unit_x", &brick->si_unit_x, NULL, },
            { 'o', "si_unit_y", &brick->si_unit_y, NULL, },
            { 'o', "si_unit_z", &brick->si_unit_z, NULL, },
            { 'o', "si_unit_w", &brick->si_unit_w, NULL, },
            { 'D', "data", &brick->data, &datasize, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_BRICK_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_brick_get_size(GObject *obj)
{
    GwyBrick *brick;
    guint32 datasize;


    gwy_debug("");
    g_return_val_if_fail(GWY_IS_BRICK(obj), 0);

    brick = GWY_BRICK(obj);

    if (!brick->si_unit_x)
        brick->si_unit_x = gwy_si_unit_new("");
    if (!brick->si_unit_y)
        brick->si_unit_y = gwy_si_unit_new("");
    if (!brick->si_unit_z)
        brick->si_unit_z = gwy_si_unit_new("");
    if (!brick->si_unit_w)
        brick->si_unit_w = gwy_si_unit_new("");

    datasize = brick->xres * brick->yres * brick->zres;

    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &brick->xres, NULL, },
            { 'i', "yres", &brick->yres, NULL, },
            { 'i', "zres", &brick->zres, NULL, },
            { 'd', "xreal", &brick->xreal, NULL, },
            { 'd', "yreal", &brick->yreal, NULL, },
            { 'd', "zreal", &brick->zreal, NULL, },
            { 'd', "xoff", &brick->xoff, NULL, },
            { 'd', "yoff", &brick->yoff, NULL, },
            { 'd', "zoff", &brick->zoff, NULL, },
            { 'o', "si_unit_x", &brick->si_unit_x, NULL, },
            { 'o', "si_unit_y", &brick->si_unit_y, NULL, },
            { 'o', "si_unit_z", &brick->si_unit_z, NULL, },
            { 'o', "si_unit_w", &brick->si_unit_w, NULL, },
            { 'D', "data", &brick->data, &datasize, },
        };

        return gwy_serialize_get_struct_size(GWY_BRICK_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_brick_deserialize(const guchar *buffer,
                      gsize size,
                      gsize *position)
{
    guint32 datasize;
    gint xres, yres, zres;
    gdouble xreal, yreal, zreal, xoff = 0.0, yoff = 0.0, zoff = 0.0, *data = NULL;
    GwySIUnit *si_unit_x = NULL, *si_unit_y = NULL, *si_unit_z = NULL, *si_unit_w = NULL;
    GwyBrick *brick;
    GwySerializeSpec spec[] = {
        { 'i', "xres", &xres, NULL, },
        { 'i', "yres", &yres, NULL, },
        { 'i', "zres", &zres, NULL, },
        { 'd', "xreal", &xreal, NULL, },
        { 'd', "yreal", &yreal, NULL, },
        { 'd', "zreal", &zreal, NULL, },
        { 'd', "xoff", &xoff, NULL, },
        { 'd', "yoff", &yoff, NULL, },
        { 'd', "zoff", &zoff, NULL, },
        { 'o', "si_unit_x", &si_unit_x, NULL, },
        { 'o', "si_unit_y", &si_unit_y, NULL, },
        { 'o', "si_unit_z", &si_unit_z, NULL, },
        { 'o', "si_unit_w", &si_unit_w, NULL, },
        { 'D', "data", &data, &datasize, },
    };


    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_BRICK_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        gwy_object_unref(si_unit_z);
        gwy_object_unref(si_unit_w);

        return NULL;
    }
    if (datasize != (guint)(xres * yres * zres)) {
        g_critical("Serialized %s size mismatch %u != %u",
                   GWY_BRICK_TYPE_NAME, datasize, xres*yres*zres);
        g_free(data);
        gwy_object_unref(si_unit_x);
        gwy_object_unref(si_unit_y);
        gwy_object_unref(si_unit_z);
        gwy_object_unref(si_unit_w);

        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    brick = gwy_brick_new(1, 1, 1, xreal, yres, zreal, FALSE);
    g_free(brick->data);
    brick->xres = xres;
    brick->yres = yres;
    brick->zres = zres;

    brick->xreal = xreal;
    brick->yreal = yreal;
    brick->zreal = zreal;

    brick->xoff = xoff;
    brick->yoff = yoff;
    brick->zoff = zoff;

    brick->data = data;
    if (si_unit_x) {
        gwy_object_unref(brick->si_unit_x);
        brick->si_unit_x = si_unit_x;
    }
    if (si_unit_y) {
        gwy_object_unref(brick->si_unit_y);
        brick->si_unit_y = si_unit_y;
    }
    if (si_unit_z) {
        gwy_object_unref(brick->si_unit_z);
        brick->si_unit_z = si_unit_z;
    }
    if (si_unit_w) {
        gwy_object_unref(brick->si_unit_w);
        brick->si_unit_w = si_unit_w;
    }

    return (GObject*)brick;
}

static GObject*
gwy_brick_duplicate_real(GObject *object)
{
    GwyBrick *brick, *duplicate;

    g_return_val_if_fail(GWY_IS_BRICK(object), NULL);
    brick = GWY_BRICK(object);
    duplicate = gwy_brick_new_alike(brick, FALSE);
    memcpy(duplicate->data, brick->data, (brick->xres * brick->yres * brick->zres)*sizeof(gdouble));

    return (GObject*)duplicate;
}

static void
gwy_brick_clone_real(GObject *source, GObject *copy)
{
    GwyBrick *brick, *clone;

    g_return_if_fail(GWY_IS_BRICK(source));
    g_return_if_fail(GWY_IS_BRICK(copy));

    brick = GWY_BRICK(source);
    clone = GWY_BRICK(copy);

    if (clone->xres != brick->xres
        || clone->yres != brick->yres
        || clone->zres != brick->zres) {
        clone->xres = brick->xres;
        clone->yres = brick->yres;
        clone->zres = brick->zres;
        clone->data = g_renew(gdouble, clone->data, clone->xres * clone->yres * clone->zres);
    }
    clone->xreal = brick->xreal;
    clone->yreal = brick->yreal;
    clone->zreal = brick->zreal;
    clone->xoff = brick->xoff;
    clone->yoff = brick->yoff;
    clone->zoff = brick->zoff;

    memcpy(clone->data, brick->data, (brick->xres * brick->yres * brick->zres)*sizeof(gdouble));

    /* SI Units can be NULL */
    if (brick->si_unit_x && clone->si_unit_x)
        gwy_serializable_clone(G_OBJECT(brick->si_unit_x),
                               G_OBJECT(clone->si_unit_x));
    else if (brick->si_unit_x && !clone->si_unit_x)
        clone->si_unit_x = gwy_si_unit_duplicate(brick->si_unit_x);
    else if (!brick->si_unit_x && clone->si_unit_x)
        gwy_object_unref(clone->si_unit_x);

    if (brick->si_unit_y && clone->si_unit_y)
        gwy_serializable_clone(G_OBJECT(brick->si_unit_y),
                               G_OBJECT(clone->si_unit_y));
    else if (brick->si_unit_y && !clone->si_unit_y)
        clone->si_unit_y = gwy_si_unit_duplicate(brick->si_unit_y);
    else if (!brick->si_unit_y && clone->si_unit_y)
        gwy_object_unref(clone->si_unit_y);

    if (brick->si_unit_z && clone->si_unit_z)
        gwy_serializable_clone(G_OBJECT(brick->si_unit_z),
                               G_OBJECT(clone->si_unit_z));
    else if (brick->si_unit_z && !clone->si_unit_z)
        clone->si_unit_z = gwy_si_unit_duplicate(brick->si_unit_z);
    else if (!brick->si_unit_z && clone->si_unit_z)
        gwy_object_unref(clone->si_unit_z);

    if (brick->si_unit_w && clone->si_unit_w)
        gwy_serializable_clone(G_OBJECT(brick->si_unit_w),
                               G_OBJECT(clone->si_unit_w));
    else if (brick->si_unit_w && !clone->si_unit_w)
        clone->si_unit_w = gwy_si_unit_duplicate(brick->si_unit_w);
    else if (!brick->si_unit_w && clone->si_unit_w)
        gwy_object_unref(clone->si_unit_w);

}

/**
 * gwy_brick_data_changed:
 * @brick: A data brick.
 *
 * Emits signal "data_changed" on a data brick.
 *
 * Since: 2.31
 **/
void
gwy_brick_data_changed(GwyBrick *brick)
{
    g_signal_emit(brick, brick_signals[DATA_CHANGED], 0);
}

/**
 * gwy_brick_resample:
 * @brick: A data brick.
 * @xres: Desired x resolution.
 * @yres: Desired y resolution.
 * @zres: Desired z resolution.
 * @interpolation: Interpolation method to use.
 *
 * Resamples a data brick.
 *
 * In other words changes the size of three dimensional field related with data
 * brick. The original values are used for resampling using a requested
 * interpolation alorithm.
 *
 * Since: 2.31
 **/
void
gwy_brick_resample(GwyBrick *brick,
                   gint xres,
                   gint yres,
                   gint zres,
                   GwyInterpolationType interpolation)
{
    gdouble *bdata, *data;
    gint row, col, lev;
    gdouble xratio, yratio, zratio;

    g_return_if_fail(GWY_IS_BRICK(brick));
    if ((xres == brick->xres) && (yres == brick->yres) && (zres == brick->zres))
        return;
    g_return_if_fail(xres > 1 && yres > 1 && zres > 1);

    if (interpolation == GWY_INTERPOLATION_NONE) {
        brick->xres = xres;
        brick->yres = yres;
        brick->zres = zres;

        brick->data = g_renew(gdouble, brick->data, brick->xres * brick->yres * brick->zres);
        return;
    }

    bdata = g_new(gdouble, brick->xres * brick->yres * brick->zres);
    data = brick->data;

    xratio = (gdouble)brick->xres/xres;
    yratio = (gdouble)brick->yres/yres;
    zratio = (gdouble)brick->zres/zres;


    if (interpolation == GWY_INTERPOLATION_ROUND) {

        for (col=0; col<xres; col++)
        {
            for (row=0; row<yres; row++)
            {
                for (lev=0; lev<zres; lev++)
                    bdata[col + xres*row + xres*yres*lev]
                        = data[MIN((gint)(xratio*col + 0.5), brick->xres-1)
                        + xres*MIN((gint)(yratio*row + 0.5), brick->yres-1)
                        + xres*yres*MIN((gint)(zratio*lev + 0.5), brick->zres-1)];
            }
        }

    }

    g_free(brick->data);
    brick->data = bdata;
    brick->xres = xres;
    brick->yres = yres;
    brick->zres = zres;

}


/**
 * gwy_brick_get_dval:
 * @brick: A data brick.
 * @x: Position in data brick in range [0, x resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @y: Position in data brick in range [0, y resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @z: Position in data brick in range [0, z resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @interpolation: Interpolation method to use.
 *
 * Gets interpolated value at arbitrary data brick point indexed by pixel
 * coordinates.
 *
 * Note pixel values are centered in intervals [@i, @i+1].
 * See also gwy_brick_get_dval_real() that does the same, but takes
 * real coordinates.
 *
 * Returns: Value interpolated in the data brick.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_dval(GwyBrick *a, gdouble x, gdouble y, gdouble z, gint interpolation)
{
    g_return_val_if_fail(GWY_IS_BRICK(a), 0.0);

    if (G_UNLIKELY(interpolation == GWY_INTERPOLATION_NONE))
        return 0.0;

    if (x<0) x = 0;
    if (y<0) y = 0;
    if (z<0) z = 0;

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        return a->data[MIN((gint)(x + 0.5), a->xres-1)
            + a->xres*MIN((gint)(y + 0.5), a->yres-1)
            + a->xres*a->yres*MIN((gint)(z + 0.5), a->zres-1)];
        break;
    }
    return 0.0;
}

/**
 * gwy_brick_get_dval_real:
 * @brick: A data brick.
 * @x: Position in data brick in range [0, x resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @y: Position in data brick in range [0, y resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @z: Position in data brick in range [0, z resolution].  If the value is outside
 *     this range, the nearest border value is returned.
 * @interpolation: Interpolation method to use.
 *
 * Gets interpolated value at arbitrary data brick point indexed by pixel
 * coordinates.
 *
 * Note pixel values are centered in intervals [@j, @j+1].
 * See also gwy_brick_get_dval() that does the same, but takes
 * pixel coordinates.
 *
 * Returns: Value interpolated in the data brick.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_dval_real(GwyBrick *a, gdouble x, gdouble y, gdouble z, gint interpolation)
{
    gdouble xratio, yratio, zratio;

    g_return_val_if_fail(GWY_IS_BRICK(a), 0.0);

    if (G_UNLIKELY(interpolation == GWY_INTERPOLATION_NONE))
        return 0.0;

    if (x<0) x = 0;
    if (y<0) y = 0;
    if (z<0) z = 0;


    xratio = a->xres/a->xreal;
    yratio = a->yres/a->yreal;
    zratio = a->zres/a->zreal;

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        return a->data[MIN((gint)(x*xratio + 0.5), a->xres-1)
            + a->xres*MIN((gint)(y*yratio + 0.5), a->yres-1)
            + a->xres*a->yres*MIN((gint)(z*zratio + 0.5), a->zres-1)];
        break;
    }
    return 0.0;
}

/**
 * gwy_brick_get_data:
 * @brick: A data brick.
 *
 * Gets the raw data buffer of a data brick.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * brick life time.  Some function may change it, most notably
 * gwy_brick_resize() and gwy_brick_resample().
 *
 * This function invalidates any cached information, use
 * gwy_brick_get_data_const() if you are not going to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_brick_get_res().
 *
 * Since: 2.31
 **/
gdouble*
gwy_brick_get_data(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    return brick->data;
}

/**
 * gwy_brick_get_data_const:
 * @brick: A data brick.
 *
 * Gets the raw data buffer of a data brick, read-only.
 *
 * The returned buffer is not guaranteed to be valid through whole data
 * brick life time.  Some function may change it, most notably
 * gwy_brick_resize() and gwy_brick_resample().
 *
 * Use gwy_brick_get_data() if you want to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_brick_get_res().
 *
 * Since: 2.31
 **/
const gdouble*
gwy_brick_get_data_const(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);
    return (const gdouble*)brick->data;
}

/**
 * gwy_brick_get_xres:
 * @brick: A data brick.
 *
 * Gets the x resolution of a data brick.
 *
 * Returns: Resolution (number of data points).
 *
 * Since: 2.31
 **/
gint
gwy_brick_get_xres(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0);
    return brick->xres;
}

/**
 * gwy_brick_get_yres:
 * @brick: A data brick.
 *
 * Gets the y resolution of a data brick.
 *
 * Returns: Resolution (number of data points).
 *
 * Since: 2.31
 **/
gint
gwy_brick_get_yres(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0);
    return brick->yres;
}
/**
 * gwy_brick_get_zres:
 * @brick: A data line.
 *
 * Gets the z resolution of a data brick.
 *
 * Returns: Resolution (number of data points).
 *
 * Since: 2.31
 **/
gint
gwy_brick_get_zres(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0);
    return brick->zres;
}

/**
 * gwy_brick_get_xreal:
 * @brick: A data brick.
 *
 * Gets the physical size of a data brick in the x direction.
 *
 * Returns: Real size of a data brick the x direction.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_xreal(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->xreal;
}

/**
 * gwy_brick_get_yreal:
 * @brick: A data brick.
 *
 * Gets the physical size of a data brick in the y direction.
 *
 * Returns: Real size of a data brick the y direction.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_yreal(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->yreal;
}
/**
 * gwy_brick_get_zreal:
 * @brick: A data brick.
 *
 * Gets the physical size of a data brick in the z direction.
 *
 * Returns: Real size of a data brick the z direction.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_zreal(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->zreal;
}

/**
 * gwy_brick_get_xoffset:
 * @brick: A data brick.
 *
 * Gets the offset of data brick origin in x direction.
 *
 * Returns: Offset value.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_xoffset(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->xoff;
}

/**
 * gwy_brick_get_yoffset:
 * @brick: A data brick.
 *
 * Gets the offset of data brick origin in y direction.
 *
 * Returns: Offset value.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_yoffset(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->yoff;
}

/**
 * gwy_brick_get_zoffset:
 * @brick: A data brick.
 *
 * Gets the offset of data brick origin in z direction.
 *
 * Returns: Offset value.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_zoffset(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), 0.0);
    return brick->zoff;
}

/**
 * gwy_brick_set_xoffset:
 * @brick: A data brick.
 * @xoffset: New offset value.
 *
 * Sets the offset of a data brick origin in the x direction.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_brick_rtoi().
 *
 * Since: 2.31
 **/
void
gwy_brick_set_xoffset(GwyBrick *brick,
                      gdouble xoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    brick->xoff = xoffset;
}

/**
 * gwy_brick_set_yoffset:
 * @brick: A data brick.
 * @yoffset: New offset value.
 *
 * Sets the offset of a data brick origin in the y direction.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_brick_rtoi().
 *
 * Since: 2.31
 **/
void
gwy_brick_set_yoffset(GwyBrick *brick,
                      gdouble yoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    brick->yoff = yoffset;
}
/**
 * gwy_brick_set_zoffset:
 * @brick: A data brick.
 * @zoffset: New offset value.
 *
 * Sets the offset of a data brick origin in the z direction.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_brick_rtoi().
 *
 * Since: 2.31
 **/
void
gwy_brick_set_zoffset(GwyBrick *brick,
                      gdouble zoffset)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    brick->zoff = zoffset;
}

/**
 * gwy_brick_set_xreal:
 * @brick: A data brick.
 * @xreal: New real x dimensions value
 *
 * Sets the real x dimension of a brick.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_xreal(GwyBrick *brick, gdouble xreal)
{
    brick->xreal = xreal;
}

/**
 * gwy_brick_set_yreal:
 * @brick: A data brick.
 * @yreal: New real y dimensions value
 *
 * Sets the real y dimension of a brick.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_yreal(GwyBrick *brick, gdouble yreal)
{
    brick->yreal = yreal;
}

/**
 * gwy_brick_set_zreal:
 * @brick: A data brick.
 * @zreal: New real z dimensions value
 *
 * Sets the real z dimension of a brick.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_zreal(GwyBrick *brick, gdouble zreal)
{
    brick->zreal = zreal;
}


/**
 * gwy_brick_get_si_unit_x:
 * @brick: A data brick.
 *
 * Returns x direction SI unit of a data brick.
 *
 * Returns: SI unit corresponding to the lateral (X) dimension of the data
 *          brick.  Its reference count is not incremented.
 *
 * Since: 2.31
 **/
GwySIUnit*
gwy_brick_get_si_unit_x(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    if (!brick->si_unit_x)
        brick->si_unit_x = gwy_si_unit_new("");

    return brick->si_unit_x;
}

/**
 * gwy_brick_get_si_unit_y:
 * @brick: A data brick.
 *
 * Returns y direction SI unit of a data brick.
 *
 * Returns: SI unit corresponding to the lateral (Y) dimension of the data
 *          brick.  Its reference count is not incremented.
 *
 * Since: 2.31
 **/
GwySIUnit*
gwy_brick_get_si_unit_y(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    if (!brick->si_unit_y)
        brick->si_unit_y = gwy_si_unit_new("");

    return brick->si_unit_y;
}

/**
 * gwy_brick_get_si_unit_z:
 * @brick: A data brick.
 *
 * Returns z direction SI unit of a data brick.
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of the data
 *          brick.  Its reference count is not incremented.
 *
 * Since: 2.31
 **/
GwySIUnit*
gwy_brick_get_si_unit_z(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    if (!brick->si_unit_z)
        brick->si_unit_z = gwy_si_unit_new("");

    return brick->si_unit_z;
}

/**
 * gwy_brick_get_si_unit_w:
 * @brick: A data brick.
 *
 * Returns value SI unit of a data brick.
 *
 * Returns: SI unit corresponding to the "value" of the data
 *          brick.  Its reference count is not incremented.
 *
 * Since: 2.31
 **/
GwySIUnit*
gwy_brick_get_si_unit_w(GwyBrick *brick)
{
    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    if (!brick->si_unit_w)
        brick->si_unit_w = gwy_si_unit_new("");

    return brick->si_unit_w;
}

/**
 * gwy_brick_set_si_unit_x:
 * @brick: A data brick.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (X) dimension of a data
 * brick.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_si_unit_x(GwyBrick *brick,
                        GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (brick->si_unit_x == si_unit)
        return;

    gwy_object_unref(brick->si_unit_x);
    g_object_ref(si_unit);
    brick->si_unit_x = si_unit;
}

/**
 * gwy_brick_set_si_unit_y:
 * @brick: A data brick.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (Y) dimension of a data
 * brick.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_si_unit_y(GwyBrick *brick,
                        GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (brick->si_unit_y == si_unit)
        return;

    gwy_object_unref(brick->si_unit_y);
    g_object_ref(si_unit);
    brick->si_unit_y = si_unit;
}

/**
 * gwy_brick_set_si_unit_z:
 * @brick: A data brick.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of a data
 * brick.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_si_unit_z(GwyBrick *brick,
                        GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (brick->si_unit_z == si_unit)
        return;

    gwy_object_unref(brick->si_unit_z);
    g_object_ref(si_unit);
    brick->si_unit_z = si_unit;
}

/**
 * gwy_brick_set_si_unit_w:
 * @brick: A data brick.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "value"  of a data
 * brick.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_si_unit_w(GwyBrick *brick,
                        GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (brick->si_unit_w == si_unit)
        return;

    gwy_object_unref(brick->si_unit_w);
    g_object_ref(si_unit);
    brick->si_unit_w = si_unit;
}

/**
 * gwy_brick_get_value_format_x:
 * @brick: A data brick.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of a data brick.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.31
 **/
GwySIValueFormat*
gwy_brick_get_value_format_x(GwyBrick *brick,
                             GwySIUnitFormatStyle style,
                             GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    max = brick->xreal;
    unit = brick->xreal/brick->xres;
    return gwy_si_unit_get_format_with_resolution
        (gwy_brick_get_si_unit_x(brick),
         style, max, unit, format);
}

/**
 * gwy_brick_get_value_format_y:
 * @brick: A data brick.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data brick.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.31
 **/
GwySIValueFormat*
gwy_brick_get_value_format_y(GwyBrick *brick,
                             GwySIUnitFormatStyle style,
                             GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    max = brick->yreal;
    unit = brick->yreal/brick->yres;
    return gwy_si_unit_get_format_with_resolution
        (gwy_brick_get_si_unit_y(brick),
         style, max, unit, format);
}

/**
 * gwy_brick_get_value_format_z:
 * @brick: A data brick.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data brick.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.31
 **/
GwySIValueFormat*
gwy_brick_get_value_format_z(GwyBrick *brick,
                             GwySIUnitFormatStyle style,
                             GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    max = brick->zreal;
    unit = brick->zreal/brick->zres;
    return gwy_si_unit_get_format_with_resolution
        (gwy_brick_get_si_unit_z(brick),
         style, max, unit, format);
}


/**
 * gwy_brick_get_min:
 * @brick: A data brick.
 *
 * Returns: The minimum value within the brick.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_min(GwyBrick *brick)
{
    gint i;
    gdouble min = G_MAXDOUBLE;

    for (i=0; i<(brick->xres * brick->yres * brick->zres); i++)
    {
        if (brick->data[i]<min) min = brick->data[i];
    }
    return min;
}

/**
 * gwy_brick_get_max:
 * @brick: A data brick.
 *
 * Returns: The maximum value within the brick.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_max(GwyBrick *brick)
{
    gint i;
    gdouble max = -G_MAXDOUBLE;

    for (i=0; i<(brick->xres * brick->yres * brick->zres); i++)
    {
        if (brick->data[i]>max) max = brick->data[i];
    }
    return max;
}

/**
 * gwy_brick_get_value_format_w:
 * @brick: A data brick.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data brick.
 *
 * Note this functions searches for minimum and maximum value in @brick,
 * therefore it's relatively slow.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 *
 * Since: 2.31
 **/
GwySIValueFormat*
gwy_brick_get_value_format_w(GwyBrick *brick,
                             GwySIUnitFormatStyle style,
                             GwySIValueFormat *format)
{
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_BRICK(brick), NULL);

    max = gwy_brick_get_max(brick);
    min = gwy_brick_get_min(brick);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }

    return gwy_si_unit_get_format(gwy_brick_get_si_unit_w(brick),
                                  style, max - min, format);
}

/**
 * gwy_brick_itor:
 * @brick: A data brick.
 * @pixpos: Pixel coordinate.
 *
 * Transforms pixel coordinate to real (physical) coordinate in x direction.
 *
 * That is it maps range [0..x resolution] to range [0..x real-size].  It is not
 * suitable for conversion of matrix indices to physical coordinates, you
 * have to use gwy_brick_itor(@brick, @pixpos + 0.5) for that.
 *
 * Returns: @pixpos in real coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_itor(GwyBrick *brick, gdouble pixpos)
{
    return pixpos * brick->xreal/brick->xres;
}

/**
 * gwy_brick_jtor:
 * @brick: A data brick.
 * @pixpos: Pixel coordinate.
 *
 * Transforms pixel coordinate to real (physical) coordinate in y direction.
 *
 * That is it maps range [0..y resolution] to range [0..y real-size].  It is not
 * suitable for conversion of matrix indices to physical coordinates, you
 * have to use gwy_brick_itor(@brick, @pixpos + 0.5) for that.
 *
 * Returns: @pixpos in real coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_jtor(GwyBrick *brick, gdouble pixpos)
{
    return pixpos * brick->yreal/brick->yres;
}
/**
 * gwy_brick_ktor:
 * @brick: A data brick.
 * @pixpos: Pixel coordinate.
 *
 * Transforms pixel coordinate to real (physical) coordinate.
 *
 * That is it maps range [0..z resolution] to range [0..z real-size].  It is not
 * suitable for conversion of matrix indices to physical coordinates, you
 * have to use gwy_brick_itor(@brick, @pixpos + 0.5) for that.
 *
 * Returns: @pixpos in real coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_ktor(GwyBrick *brick, gdouble pixpos)
{
    return pixpos * brick->zreal/brick->zres;
}

/**
 * gwy_brick_rtoi:
 * @brick: A data brick.
 * @realpos: Real coordinate.
 *
 * Transforms real (physical) coordinate to pixel coordinate in x axis.
 *
 * That is it maps range [0..x real-size] to range [0..x resolution].
 *
 * Returns: @realpos in pixel coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_rtoi(GwyBrick *brick, gdouble realpos)
{
    return realpos * brick->xres/brick->xreal;
}

/**
 * gwy_brick_rtoj:
 * @brick: A data brick.
 * @realpos: Real coordinate.
 *
 * Transforms real (physical) coordinate to pixel coordinate in y axis.
 *
 * That is it maps range [0..y real-size] to range [0..y resolution].
 *
 * Returns: @realpos in pixel coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_rtoj(GwyBrick *brick, gdouble realpos)
{
    return realpos * brick->yres/brick->yreal;
}

/**
 * gwy_brick_rtok:
 * @brick: A data brick.
 * @realpos: Real coordinate.
 *
 * Transforms real (physical) coordinate to pixel coordinate in z axis.
 *
 * That is it maps range [0..z real-size] to range [0..z resolution].
 *
 * Returns: @realpos in pixel coordinates.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_rtok(GwyBrick *brick, gdouble realpos)
{
    return realpos * brick->zres/brick->zreal;
}

/**
 * gwy_brick_get_val:
 * @brick: A data brick.
 * @col: Position in the brick (column index).
 * @row: Position in the brick (row index).
 * @lev: Position in the brick (level index).
 *
 * Gets value at given position in a data brick.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_brick_get_data_const() and access it
 * directly instead.
 *
 * Returns: Value at given index.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_val(GwyBrick *brick,
                  gint col,
                  gint row,
                  gint lev)
{
    g_return_val_if_fail(col >= 0 && col < (brick->xres)
                         && row>=0 && row < (brick->yres)
                         && lev>=0 && lev < (brick->zres), 0.0);

    return brick->data[col + brick->xres*row + brick->xres*brick->yres*lev];
}

/**
 * gwy_brick_set_val:
 * @brick: A data brick.
 * @col: Position in the brick (column index).
 * @row: Position in the brick (row index).
 * @lev: Position in the brick (level index).
 * @value: value to be set.
 *
 * Sets value at given position in a data brick.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_brick_get_data_const() and access it
 * directly instead.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_val(GwyBrick *brick,
                  gint col,
                  gint row,
                  gint lev,
                  gdouble value)
{
    g_return_if_fail(col >= 0 && col < (brick->xres)
                     && row>=0 && row < (brick->yres)
                     && lev>=0 && lev < (brick->zres));

    brick->data[col + brick->xres*row + brick->xres*brick->yres*lev] = value;
}

/**
 * gwy_brick_get_val_real:
 * @brick: A data brick.
 * @x: Position in the brick (x direction).
 * @y: Position in the brick (y direction).
 * @z: Position in the brick (z direction).
 *
 * Gets value at given position in a data brick, in real coordinates.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_brick_get_data_const() and access it
 * directly instead.
 *
 * Returns: Value at given index.
 *
 * Since: 2.31
 **/
gdouble
gwy_brick_get_val_real(GwyBrick *brick,
                       gdouble x,
                       gdouble y,
                       gdouble z)
{
    gint col = gwy_brick_rtoi(brick, x);
    gint row = gwy_brick_rtoj(brick, y);
    gint lev = gwy_brick_rtok(brick, z);

    g_return_val_if_fail(col >= 0 && col < (brick->xres)
                         && row>=0 && row < (brick->yres)
                         && lev>=0 && lev < (brick->zres), 0.0);

    return brick->data[col + brick->xres*row + brick->xres*brick->yres*lev];
}

/**
 * gwy_brick_set_val_real:
 * @brick: A data brick.
 * @x: Position in the brick (x direction).
 * @y: Position in the brick (y direction).
 * @z: Position in the brick (z direction).
 *
 * Sets value at given position in a data brick.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_brick_get_data_const() and access it
 * directly instead.
 *
 * Since: 2.31
 **/
void
gwy_brick_set_val_real(GwyBrick *brick,
                       gdouble x,
                       gdouble y,
                       gdouble z,
                       gdouble value)
{
    gint col = gwy_brick_rtoi(brick, x);
    gint row = gwy_brick_rtoj(brick, y);
    gint lev = gwy_brick_rtok(brick, z);

    g_return_if_fail(col >= 0 && col < (brick->xres)
                     && row>=0 && row < (brick->yres)
                     && lev>=0 && lev < (brick->zres));

    brick->data[col + brick->xres*row + brick->xres*brick->yres*lev] = value;
}



/**
 * gwy_brick_fill:
 * @brick: A data brick.
 * @value: Value to fill data brick with.
 *
 * Fills a data brick with specified value.
 *
 * Since: 2.31
 **/
void
gwy_brick_fill(GwyBrick *brick,
               gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_BRICK(brick));
    for (i = 0; i < (brick->xres*brick->yres*brick->zres); i++)
        brick->data[i] = value;
}

/**
 * gwy_brick_clear:
 * @brick: A data brick.
 *
 * Fills a data brick with zeroes.
 *
 * Since: 2.31
 **/
void
gwy_brick_clear(GwyBrick *brick)
{
    g_return_if_fail(GWY_IS_BRICK(brick));
    gwy_clear(brick->data, brick->xres*brick->yres*brick->zres);
}

/**
 * gwy_brick_add:
 * @brick: A data brick.
 * @value: Value to be added.
 *
 * Adds a specified value to all values in a data brick.
 *
 * Since: 2.31
 **/
void
gwy_brick_add(GwyBrick *brick,
              gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_BRICK(brick));
    for (i = 0; i < (brick->xres*brick->yres*brick->zres); i++)
        brick->data[i] += value;
}

/**
 * gwy_brick_multiply:
 * @brick: A data brick.
 * @value: Value to multiply data brick with.
 *
 * Multiplies all values in a data brick with a specified value.
 *
 * Since: 2.31
 **/
void
gwy_brick_multiply(GwyBrick *brick,
                   gdouble value)
{
    gint i;

    g_return_if_fail(GWY_IS_BRICK(brick));
    for (i = 0; i < (brick->xres*brick->yres*brick->zres); i++)
        brick->data[i] *= value;
}

/**
 * gwy_brick_extract_plane:
 * @brick: A data brick.
 * @target: Datafield to be filled by extracted plane. It will be resampled if necessary.
 * @istart: column where to start (pixel coordinates).
 * @jstart: row where to start (pixel coordinates).
 * @kstart: level where to start (pixel coordinates).
 * @width: pixel width of extracted plane. If @width is -1, the yz plane will be extracted.
 * @height: pixel height of extracted plane.  If @height is -1, the xz plane will be extracted
 * @depth: pixel depth of extacted plane. If @depth is -1, the xy plane will be extracted
 * @keep_offsets: keep the physical offsets in extracted field.
 *
 * Extract a plane (GwyDataField) from the brick. One value of set (@width, @height, @depth) needs
 * to be -1, determining the plane orientation.
 *
 * Since: 2.31
 **/
void
gwy_brick_extract_plane(const GwyBrick *brick,
                        GwyDataField *target,
                        gint istart,
                        gint jstart,
                        gint kstart,
                        gint width,
                        gint height,
                        gint depth,
                        G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width==-1 && height>0 && depth>0) ||
                     (width>0 && height==-1 && depth>0) ||
                     (width>0 && height>0 && depth==-1));

    g_return_if_fail(istart>=0 && istart<brick->xres &&
                     jstart>=0 && jstart<brick->yres &&
                     kstart>=0 && kstart<brick->zres);

    bdata = brick->data;


    if (width==-1 && height>0 && depth>0)
    {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, height, depth, GWY_INTERPOLATION_NONE);
        ddata = gwy_data_field_get_data(target);
        gwy_data_field_set_xreal(target, brick->yreal);
        gwy_data_field_set_yreal(target, brick->zreal);


        col = istart;
        for (row = 0; row<height; row++)
        {
            for (lev = 0; lev<depth; lev++)
            {
                ddata[row + lev*height] = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev + kstart)];
            }
        }
    }

    if (width>0 && height==-1 && depth>0)
    {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, width, depth, GWY_INTERPOLATION_NONE);
        ddata = gwy_data_field_get_data(target);
        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        row = jstart;
        for (col = 0; col<width; col++)
        {
            for (lev = 0; lev<depth; lev++)
            {
                ddata[col + lev*width] = bdata[col + istart + brick->xres*row + brick->xres*brick->yres*(lev + kstart)];
            }
        }
    }

    if (width>0 && height>0 && depth==-1)
    {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        gwy_data_field_resample(target, width, height, GWY_INTERPOLATION_NONE);
        ddata = gwy_data_field_get_data(target);
        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->yreal);


        lev = kstart;
        for (col = 0; col<width; col++)
        {
            for (row = 0; row<height; row++)
            {
                ddata[col + row*width] = bdata[col + istart + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
            }
        }
    }


}


/**
 * gwy_brick_sum_plane:
 * @brick: A data brick.
 * @target: Datafield to be filled by summed plane. It will be resampled if
 *          necessary.
 * @istart: column where to start (pixel coordinates).
 * @jstart: row where to start (pixel coordinates).
 * @kstart: level where to start (pixel coordinates).
 * @width: pixel width of summed plane. If @width is -1, the yz planes will be
 *         summed.
 * @height: pixel height of summed plane.  If @height is -1, the xz planes will
 *          be summed
 * @depth: pixel depth of extacted plane. If @depth is -1, the xy planes will
 *         be summed
 * @keep_offsets: keep the physical offsets in extracted field.  Not
 *                implemented.
 *
 * Sums planes in certain direction and extract the result (GwyDataField). One
 * value of set (@width, @height, @depth) needs to be -1, determining the plane
 * orientation. In contrast to gwy_brick_extract_plane, the appropriate start
 * coordinate (e.g. @istart if @width = -1) is not used for single plane
 * extraction, but the planes are accumulated in whole range (0..xres for given
 * example)
 *
 * Since: 2.31
 **/
void
gwy_brick_sum_plane(const GwyBrick *brick,
                    GwyDataField *target,
                    gint istart,
                    gint jstart,
                    gint kstart,
                    gint width,
                    gint height,
                    gint depth,
                    G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width == -1 && height > 0 && depth > 0)
                     || (width > 0 && height == -1 && depth > 0)
                     || (width > 0 && height > 0 && depth == -1));

    g_return_if_fail(istart >=0 && istart < brick->xres
                     && jstart >=0 && jstart < brick->yres
                     && kstart >=0 && kstart < brick->zres);

    bdata = brick->data;

    if (width == -1 && height > 0 && depth > 0) {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, height, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->yreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                for (col = 0; col < brick->xres; col++) {
                    gdouble bv = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev + kstart)];
                    ddata[row + lev*height] += bv;
                }
            }
        }
    }

    if (width > 0 && height == -1 && depth > 0) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, width, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                for (row = 0; row < brick->yres; row++) {
                    gdouble bv = bdata[col + istart + brick->xres*row + brick->xres*brick->yres*(lev + kstart)];
                    ddata[col + lev*width] += bv;
                }
            }
        }
    }

    if (width > 0 && height > 0 && depth == -1) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        gwy_data_field_resample(target, width, height, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->yreal);

        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                for (lev = 0; lev < brick->zres; lev++) {
                    gdouble bv = bdata[col + istart + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
                    ddata[col + row*width] += bv;
                }
            }
        }
    }
}

void
gwy_brick_min_plane(const GwyBrick *brick,
                    GwyDataField *target,
                    gint istart,
                    gint jstart,
                    gint kstart,
                    gint width,
                    gint height,
                    gint depth,
                    G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width == -1 && height > 0 && depth > 0)
                     || (width > 0 && height == -1 && depth > 0)
                     || (width > 0 && height > 0 && depth == -1));

    g_return_if_fail(istart >=0 && istart < brick->xres
                     && jstart >=0 && jstart < brick->yres
                     && kstart >=0 && kstart < brick->zres);

    bdata = brick->data;
    gwy_data_field_clear(target);

    if (width == -1 && height > 0 && depth > 0) {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, height, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MAXDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->yreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                for (col = 0; col < brick->xres; col++) {
                    gdouble bv = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev + kstart)];
                    gdouble fv = ddata[row + lev*height];
                    ddata[row + lev*height] = MIN(bv, fv);
                }
            }
        }
    }

    if (width > 0 && height == -1 && depth > 0) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, width, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MAXDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                for (row = 0; row < brick->yres; row++) {
                    gdouble bv = bdata[col + istart + brick->xres*row + brick->xres*brick->yres*(lev + kstart)];
                    gdouble fv = ddata[col + lev*width];
                    ddata[col + lev*width] = MIN(bv, fv);
                }
            }
        }
    }

    if (width > 0 && height > 0 && depth == -1) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        gwy_data_field_resample(target, width, height, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MAXDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->yreal);

        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                for (lev = 0; lev < brick->zres; lev++) {
                    gdouble bv = bdata[col + istart + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
                    gdouble fv = ddata[col + row*width];
                    ddata[col + row*width] = MIN(bv, fv);
                }
            }
        }
    }
}

void
gwy_brick_max_plane(const GwyBrick *brick,
                    GwyDataField *target,
                    gint istart,
                    gint jstart,
                    gint kstart,
                    gint width,
                    gint height,
                    gint depth,
                    G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width == -1 && height > 0 && depth > 0)
                     || (width > 0 && height == -1 && depth > 0)
                     || (width > 0 && height > 0 && depth == -1));

    g_return_if_fail(istart >=0 && istart < brick->xres
                     && jstart >=0 && jstart < brick->yres
                     && kstart >=0 && kstart < brick->zres);

    bdata = brick->data;
    gwy_data_field_clear(target);

    if (width == -1 && height > 0 && depth > 0) {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, height, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MINDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->yreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                for (col = 0; col < brick->xres; col++) {
                    gdouble bv = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev + kstart)];
                    gdouble fv = ddata[row + lev*height];
                    ddata[row + lev*height] = MAX(bv, fv);
                }
            }
        }
    }

    if (width > 0 && height == -1 && depth > 0) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, width, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MINDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                for (row = 0; row < brick->yres; row++) {
                    gdouble bv = bdata[col + istart + brick->xres*row + brick->xres*brick->yres*(lev + kstart)];
                    gdouble fv = ddata[col + lev*width];
                    ddata[col + lev*width] = MAX(bv, fv);
                }
            }
        }
    }

    if (width > 0 && height > 0 && depth == -1) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        gwy_data_field_resample(target, width, height, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(target, G_MINDOUBLE);
        ddata = gwy_data_field_get_data(target);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->yreal);

        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                for (lev = 0; lev < brick->zres; lev++) {
                    gdouble bv = bdata[col + istart + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
                    gdouble fv = ddata[col + row*width];
                    ddata[col + row*width] = MAX(bv, fv);
                }
            }
        }
    }
}

void
gwy_brick_mean_plane(const GwyBrick *brick,
                     GwyDataField *target,
                     gint istart,
                     gint jstart,
                     gint kstart,
                     gint width,
                     gint height,
                     gint depth,
                     G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width == -1 && height > 0 && depth > 0)
                     || (width > 0 && height == -1 && depth > 0)
                     || (width > 0 && height > 0 && depth == -1));

    g_return_if_fail(istart >=0 && istart < brick->xres
                     && jstart >=0 && jstart < brick->yres
                     && kstart >=0 && kstart < brick->zres);

    gwy_brick_sum_plane(brick, target,
                        istart, jstart, kstart, width, height, depth,
                        keep_offsets);

    if (width == -1 && height > 0 && depth > 0) {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        ddata = gwy_data_field_get_data(target);
        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                ddata[row + lev*height] /= brick->xres;
            }
        }
    }

    if (width > 0 && height == -1 && depth > 0) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        ddata = gwy_data_field_get_data(target);
        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                ddata[col + lev*width] /= brick->yres;
            }
        }
    }

    if (width > 0 && height > 0 && depth == -1) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        ddata = gwy_data_field_get_data(target);
        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                ddata[col + row*width] /= brick->zres;
            }
        }
    }
}

void
gwy_brick_rms_plane(const GwyBrick *brick,
                    GwyDataField *target,
                    gint istart,
                    gint jstart,
                    gint kstart,
                    gint width,
                    gint height,
                    gint depth,
                    G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata, *mdata;
    GwyDataField *meanfield;

    g_return_if_fail(GWY_IS_BRICK(brick));

    g_return_if_fail((width == -1 && height > 0 && depth > 0)
                     || (width > 0 && height == -1 && depth > 0)
                     || (width > 0 && height > 0 && depth == -1));

    g_return_if_fail(istart >=0 && istart < brick->xres
                     && jstart >=0 && jstart < brick->yres
                     && kstart >=0 && kstart < brick->zres);

    meanfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
    gwy_brick_mean_plane(brick, meanfield,
                         istart, jstart, kstart, width, height, depth,
                         keep_offsets);

    bdata = brick->data;

    if (width == -1 && height > 0 && depth > 0) {
        g_return_if_fail((jstart+height) <= brick->yres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, height, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);
        mdata = gwy_data_field_get_data(meanfield);

        gwy_data_field_set_xreal(target, brick->yreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                for (col = 0; col < brick->xres; col++) {
                    gdouble bv = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev + kstart)];
                    gdouble mv = mdata[row + lev*height];
                    ddata[row + lev*height] += (bv - mv)*(bv - mv);
                }
            }
        }

        for (row = 0; row < height; row++) {
            for (lev = 0; lev < depth; lev++) {
                ddata[row + lev*height] /= brick->xres;
                ddata[row + lev*height] = sqrt(ddata[row + lev*height]);
            }
        }
    }

    if (width > 0 && height == -1 && depth > 0) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((kstart+depth) <= brick->zres);

        gwy_data_field_resample(target, width, depth, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);
        mdata = gwy_data_field_get_data(meanfield);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->zreal);

        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                for (row = 0; row < brick->yres; row++) {
                    gdouble bv = bdata[col + istart + brick->xres*row + brick->xres*brick->yres*(lev + kstart)];
                    gdouble mv = mdata[col + lev*width];
                    ddata[col + lev*width] += (bv - mv)*(bv - mv);
                }
            }
        }

        for (col = 0; col < width; col++) {
            for (lev = 0; lev < depth; lev++) {
                ddata[col + lev*width] /= brick->yres;
                ddata[col + lev*width] = sqrt(ddata[col + lev*width]);
            }
        }
    }

    if (width > 0 && height > 0 && depth == -1) {
        g_return_if_fail((istart+width) <= brick->xres);
        g_return_if_fail((jstart+height) <= brick->yres);

        gwy_data_field_resample(target, width, height, GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(target);
        ddata = gwy_data_field_get_data(target);
        mdata = gwy_data_field_get_data(meanfield);

        gwy_data_field_set_xreal(target, brick->xreal);
        gwy_data_field_set_yreal(target, brick->yreal);

        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                for (lev = 0; lev < brick->zres; lev++) {
                    gdouble bv = bdata[col + istart + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
                    gdouble mv = mdata[col + row*width];
                    ddata[col + row*width] += (bv - mv)*(bv - mv);
                }
            }
        }

        for (col = 0; col < width; col++) {
            for (row = 0; row < height; row++) {
                ddata[col + row*width] /= brick->zres;
                ddata[col + row*width] = sqrt(ddata[col + row*width]);
            }
        }
    }

    g_object_unref(meanfield);
}

/**
 * gwy_brick_extract_line:
 * @brick: A data brick.
 * @target: Dataline to be filled by extracted line. It will be resampled if necessary.
 * @istart: column where to start (pixel coordinates).
 * @jstart: row where to start (pixel coordinates).
 * @kstart: level where to start (pixel coordinates).
 * @iend: column where to start (pixel coordinates).
 * @jend: row where to start (pixel coordinates).
 * @kend: level where to start (pixel coordinates).
 * @keep_offsets: keep physical offsets in extracted line
 *
 * Extract a line (GwyDataField) from the brick. Only line orientations parallel to coordinate
 * axes are supported now, i.e. two of the start coordinates need to be same as end ones.
 *
 * Since: 2.31
 **/

void
gwy_brick_extract_line(const GwyBrick *brick, GwyDataLine *target,
                       gint istart, gint jstart, gint kstart,
                       gint iend, gint jend, gint kend,
                       G_GNUC_UNUSED gboolean keep_offsets)
{
    gint col, row, lev;
    gdouble *bdata, *ddata;

    g_return_if_fail(GWY_IS_BRICK(brick));


    g_return_if_fail(istart>=0 && istart<=brick->xres &&
                     jstart>=0 && jstart<=brick->yres &&
                     kstart>=0 && kstart<=brick->zres &&
                     iend>=0 && iend<=brick->xres &&
                     jend>=0 && jend<=brick->yres &&
                     kend>=0 && kend<=brick->zres);

    bdata = brick->data;


    if ((jstart == jend) && (kstart == kend))
    {
        gwy_data_line_resample(target, abs(iend-istart), GWY_INTERPOLATION_NONE);
        ddata = gwy_data_line_get_data(target);

        row = jstart;
        lev = kstart;
        if (iend>=istart)
            for (col = 0; col<(iend-istart); col++)
            {
                ddata[col] = bdata[col + istart + brick->xres*(row) + brick->xres*brick->yres*(lev)];
            }
        else
            for (col = 0; col<(istart-iend); col++)
            {
                ddata[col] = bdata[iend - col - 1 + brick->xres*(row) + brick->xres*brick->yres*(lev)];
            }
    }

    if ((istart == iend) && (kstart == kend))
    {
        gwy_data_line_resample(target, abs(jend-jstart), GWY_INTERPOLATION_NONE);
        ddata = gwy_data_line_get_data(target);

        col = istart;
        lev = kstart;
        if (jend>=jstart)
            for (row = 0; row<(jend-jstart); row++)
            {
                ddata[row] = bdata[col + brick->xres*(row + jstart) + brick->xres*brick->yres*(lev)];
            }
        else
            for (row = 0; row<(jstart-jend); row++)
            {
                ddata[row] = bdata[col + brick->xres*(jstart - row - 1) + brick->xres*brick->yres*(lev)];
            }
    }

    if ((istart == iend) && (jstart == jend))
    {
        gwy_data_line_resample(target, abs(kend-kstart), GWY_INTERPOLATION_NONE);
        ddata = gwy_data_line_get_data(target);

        col = istart;
        row = jstart;
        if (kend>=kstart)
            for (lev = 0; lev<(kend-kstart); lev++)
            {
                ddata[lev] = bdata[col + brick->xres*(row) + brick->xres*brick->yres*(lev + kstart)];
            }
        else
            for (lev = 0; lev<(kstart-kend); lev++)
            {
                ddata[lev] = bdata[col + brick->xres*(row) + brick->xres*brick->yres*(kend - lev - 1)];
            }
    }


}

/************************** Documentation ****************************/

/**
 * SECTION:brick
 * @title: GwyBrick
 * @short_description: Three-dimensioanl data representation
 *
 * #GwyBrick represents 3D data arrays in Gwyddion. It is typically useful for
 * different volume data obtained from SPMs, like in force volume measurements.
 **/

/**
 * GwyBrick:
 *
 * The #GwyBrick struct contains private data only and should be accessed
 * using the functions below.
 *
 * Since: 2.31
 **/

/**
 * gwy_brick_duplicate:
 * @brick: A data brick to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 *
 * Since: 2.31
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
