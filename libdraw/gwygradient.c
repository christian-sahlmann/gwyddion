/*
 *  @(#) $Id$
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

/* TODO:
 * - reduce the number of system palettes
 * - set from samples
 * - changes, signal emission
 * - fast get_color (with hinting)
 * - test
 */

#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwygradient.h"
#include "gwypalettedef.h"

#define GWY_GRADIENT_TYPE_NAME "GwyGradient"

enum {
    GWY_GRADIENT_DEFAULT_SIZE = 512
};

#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))

static void         gwy_gradient_class_init       (GwyGradientClass *klass);
static void         gwy_gradient_init             (GwyGradient *gradient);
static void         gwy_gradient_finalize         (GObject *object);
static void         gwy_gradient_serializable_init(GwySerializableIface *iface);
static void         gwy_gradient_watchable_init   (GwyWatchableIface *iface);
static void         gwy_gradient_sanitize         (GwyGradient *gradient);
static void         gwy_gradient_changed          (GwyGradient *gradient);
static gchar*       gwy_gradient_invent_name      (GHashTable *gradients,
                                                   const gchar *prefix,
                                                   gboolean warn);
static GwyGradient* gwy_gradient_new_internal   (gchar *name,
                                                 gint npoints,
                                                 const GwyGradientPoint *points,
                                                 gboolean modifiable,
                                                 gboolean do_sample);

/* TODO */
static GByteArray* gwy_gradient_serialize          (GObject *obj,
                                                   GByteArray *buffer);
static GObject* gwy_gradient_deserialize           (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject* gwy_gradient_duplicate             (GObject *object);

static const gchar *magic_header = "Gwyddion Gradient 1.0";

static const GwyRGBA null_color = { 0, 0, 0, 0 };
static const GwyRGBA black_color = { 0, 0, 0, 1 };
static const GwyRGBA white_color = { 1, 1, 1, 1 };
static const GwyRGBA red_color = { 1, 0, 0, 1 };
static const GwyRGBA green_color = { 0, 1, 0, 1 };
static const GwyRGBA blue_color = { 0, 0, 1, 1 };
static const GwyRGBA cyan_color = { 0, 1, 1, 1 };
static const GwyRGBA violet_color = { 1, 0, 1, 1 };
static const GwyRGBA yellow_color = { 1, 1, 0, 1 };
static const GwyGradientPoint null_point = { 0, { 0, 0, 0, 0 } };

static GObjectClass *parent_class = NULL;

GType
gwy_gradient_get_type(void)
{
    static GType gwy_gradient_type = 0;

    if (!gwy_gradient_type) {
        static const GTypeInfo gwy_gradient_info = {
            sizeof(GwyGradientClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_gradient_class_init,
            NULL,
            NULL,
            sizeof(GwyGradient),
            0,
            (GInstanceInitFunc)gwy_gradient_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_gradient_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_gradient_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_gradient_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_GRADIENT_TYPE_NAME,
                                                   &gwy_gradient_info,
                                                   0);
        g_type_add_interface_static(gwy_gradient_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_gradient_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_gradient_type;
}

static void
gwy_gradient_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_gradient_serialize;
    iface->deserialize = gwy_gradient_deserialize;
    iface->duplicate = gwy_gradient_duplicate;
}

static void
gwy_gradient_watchable_init(GwyWatchableIface *iface)
{
    iface->value_changed = NULL;
}

static void
gwy_gradient_class_init(GwyGradientClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_gradient_finalize;
    klass->gradients = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             NULL, g_object_unref);
}

static void
gwy_gradient_init(GwyGradient *gradient)
{
    gradient->modifiable = TRUE;
    gwy_debug_objects_creation((GObject*)gradient);
}

static void
gwy_gradient_finalize(GObject *object)
{
    GwyGradient *gradient = (GwyGradient*)object;

    gwy_debug("%s", gradient->name);

    g_array_free(gradient->points, TRUE);
    g_free(gradient->pixels);
    g_free(gradient->name);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_gradient_get_name:
 * @gradient: A color gradient.
 *
 * Returns color gradient name.
 *
 * Returns: Name of @gradient.  The string is owned by @gradient and must not
 *          be modfied or freed.
 *
 * Since: 1.7
 **/
const gchar*
gwy_gradient_get_name(GwyGradient *gradient)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    return gradient->name;
}

/**
 * gwy_gradient_is_modifiable:
 * @gradient: A color gradient.
 *
 * Returns whether a color gradient is modifiable.
 *
 * It is an error to try to insert, delete, or set points in fixed (system)
 * gradients, or to try to delete them.
 *
 * Returns: %TRUE if gradient is modifiable, %FALSE if it's system gradient.
 *
 * Since: 1.7
 **/
gboolean
gwy_gradient_is_modifiable(GwyGradient *gradient)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), FALSE);
    return gradient->modifiable;
}

/**
 * gwy_palette_def_get_color:
 * @gradient: A color gradient.
 * @x: Position in gradient, in range 0..1.
 *
 * Computes color at given position of a color gradient.
 *
 * Returns: The interpolated color sample.
 *
 * Since: 1.7
 **/
GwyRGBA
gwy_gradient_get_color(GwyGradient *gradient,
                       gdouble x)
{
    GwyRGBA ret;
    GArray *points;
    GwyGradientPoint *pt = NULL, *pt2;
    guint i;

    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), null_color);
    g_return_val_if_fail(x >= 0.0 && x <= 1.0, null_color);

    points = gradient->points;
    if (x == 1.0)
        return g_array_index(points, GwyGradientPoint, points->len-1).color;

    /* find the right subinterval */
    for (i = 0; i < points->len-1; i++) {
        pt = &g_array_index(points, GwyGradientPoint, i);
        if (pt->x == x)
            return pt->color;
        if (pt->x < x)
            break;
    }
    g_assert(i < points->len-1);
    pt2 = &g_array_index(points, GwyGradientPoint, i+1);

    gwy_rgba_interpolate(&pt->color, &pt2->color, (x - pt->x)/(pt2->x - pt->x),
                         &ret);

    return ret;
}

/**
 * gwy_gradient_get_samples:
 * @gradient: A color gradient to get samples of.
 * @nsamples: A location to store the number of samples (or %NULL).
 *
 * Returns color gradient sampled to integers in #GdkPixbuf-like scheme.
 *
 * The returned samples are owned by @gradient and must not be modified or
 * freed.  Their contents changes when the gradient changes, although their
 * number never changes.  The returned pointer is valid only during
 * @gradient lifetime, for system gradient it means practically always, but
 * user-defined gradients may be destroyed.
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 *
 * Since: 1.7
 **/
const guchar*
gwy_gradient_get_samples(GwyGradient *gradient,
                         gint *nsamples)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    if (*nsamples)
        *nsamples = GWY_GRADIENT_DEFAULT_SIZE;
    return gradient->pixels;
}

/**
 * gwy_gradient_sample:
 * @gradient: A color gradient to sample.
 * @nsamples: Required number of samples.
 * @samples: Pointer to array to be filled.
 *
 * Samples gradient to an array #GdkPixbuf-like samples.
 *
 * If @samples is not %NULL, it's resized to 4*@nsamples bytes, otherwise a
 * new buffer is allocated.
 *
 * If you don't have a reason for specific sample size (and are not going
 * to modify the samples or otherwise dislike the automatic resampling on
 * gradient definition change), use gwy_gradient_get_samples() instead.
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 *
 * Since: 1.7
 **/
guchar*
gwy_gradient_sample(GwyGradient *gradient,
                    gint nsamples,
                    guchar *samples)
{
    gint i, k;
    gdouble q;
    GwyRGBA color;

    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    g_return_val_if_fail(nsamples > 1, NULL);

    samples = g_renew(guchar, samples, 4*nsamples);

    q = 1.0/(nsamples - 1.0);
    for (i = k = 0; i < nsamples; i++) {
        /* FIXME: this is slow for gradients with many colors.  Use hints
         * to find the color faster */
        color = gwy_gradient_get_color(gradient, i*q);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.r);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.g);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.b);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.a);
    }

    return samples;
}

/**
 * gwy_gradient_get_npoints:
 * @gradient: A color gradient.
 *
 * Returns the number of points in a gradient.
 *
 * Returns: The number of points in @gradient.
 *
 * Since: 1.7
 **/
gint
gwy_gradient_get_npoints(GwyGradient *gradient)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), 0);
    return gradient->points->len;
}

/**
 * gwy_gradient_get_point:
 * @gradient: A color gradient.
 * @index_: Color point index in @gradient.
 *
 * Returns point at given index of a color gradient.
 *
 * Returns: Color point at @index_.
 *
 * Since: 1.7
 **/
GwyGradientPoint
gwy_gradient_get_point(GwyGradient *gradient,
                       gint index_)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), null_point);
    g_return_val_if_fail(index_ >= 0 && index_ < gradient->points->len,
                         null_point);
    return g_array_index(gradient->points, GwyGradientPoint, index_);
}

static inline GwyRGBA
gwy_gradient_fix_rgba(const GwyRGBA *color)
{
    GwyRGBA rgba;

    rgba.r = CLAMP(color->r, 0.0, 1.0);
    rgba.g = CLAMP(color->g, 0.0, 1.0);
    rgba.b = CLAMP(color->b, 0.0, 1.0);
    rgba.a = CLAMP(color->a, 0.0, 1.0);
    if (rgba.r != color->r
        || rgba.g != color->g
        || rgba.b != color->b
        || rgba.a != color->a)
        g_warning("Color component outside 0..1 range");

    return rgba;
}

static inline gdouble
gwy_gradient_fix_position(GArray *points,
                          gint i,
                          gdouble pos)
{
    gdouble xprec, xsucc, x;

    if (i == 0)
        xprec = xsucc = 0.0;
    else if (i == points->len-1)
        xprec = xsucc = 1.0;
    else {
        xprec = g_array_index(points, GwyGradientPoint, i-1).x;
        xsucc = g_array_index(points, GwyGradientPoint, i+1).x;
    }
    x = CLAMP(pos, xprec, xsucc);
    if (x != pos)
        g_warning("Point beyond neighbours or outside 0..1");

    return x;
}

/**
 * gwy_gradient_set_point:
 * @gradient: A color gradient.
 * @index_: Color point index in @gradient.
 * @point: Color point to replace current point at @index_ with.
 *
 * Sets a single color point in a color gradient.
 *
 * It is an error to try to move points beyond is neighbours, or to move first
 * (or last) point from 0 (or 1).
 *
 * Since: 1.7
 **/
void
gwy_gradient_set_point(GwyGradient *gradient,
                       gint index_,
                       const GwyGradientPoint *point)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(point);
    g_return_if_fail(index_ >= 0 && index_ < gradient->points->len);

    pt.color = gwy_gradient_fix_rgba(&point->color);
    pt.x = gwy_gradient_fix_position(gradient->points, index_, point->x);
    g_array_index(gradient->points, GwyGradientPoint, index_) = pt;

    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_set_point_color:
 * @gradient: A color gradient.
 * @index_: Color point index in @gradient.
 * @color: Color to set the point to.
 *
 * Sets a color of color gradient point without moving it.
 *
 * Since: 1.7
 **/
void
gwy_gradient_set_point_color(GwyGradient *gradient,
                             gint index_,
                             const GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(color);
    g_return_if_fail(index_ >= 0 && index_ < gradient->points->len);

    g_array_index(gradient->points, GwyGradientPoint, index_).color
        = gwy_gradient_fix_rgba(color);;

    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_insert_point:
 * @gradient: A color gradient.
 * @index_: Color point index in @gradient.
 * @point: Color point to insert at @index_.
 *
 * Inserts a point to a color gradient.
 *
 * It is an error to try to position a outside its future neighbours, or to
 * move first (or last) point from 0 (or 1).
 *
 * Since: 1.7
 **/
void
gwy_gradient_insert_point(GwyGradient *gradient,
                          gint index_,
                          GwyGradientPoint *point)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(point);
    g_return_if_fail(index_ >= 0 && index_ <= gradient->points->len);

    pt.color = gwy_gradient_fix_rgba(&point->color);
    if (index_ == gradient->points->len) {
        pt.x = 1.0;
        if (point->x != 1.0)
            g_warning("Point beyond neighbours or outside 0..1");
        g_array_append_val(gradient->points, pt);
        gwy_gradient_changed(gradient);
        return;
    }
    if (index_ == 0) {
        pt.x = 0.0;
        if (point->x != 0.0)
            g_warning("Point beyond neighbours or outside 0..1");
        gwy_gradient_changed(gradient);
        g_array_prepend_val(gradient->points, pt);
    }

    /* duplicate point at index_ for position checking */
    g_array_insert_val(gradient->points, index_,
                       g_array_index(gradient->points, GwyGradientPoint,
                                     index_));
    pt.x = gwy_gradient_fix_position(gradient->points, index_, point->x);
    g_array_index(gradient->points, GwyGradientPoint, index_) = pt;

    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_insert_point_sorted:
 * @gradient: A color gradient.
 * @point: Color point to insert.
 *
 * Inserts a point into color gradient based on its x position.
 *
 * Returns: The index @point was inserted at.
 *
 * Since: 1.7
 **/
gint
gwy_gradient_insert_point_sorted(GwyGradient *gradient,
                                 GwyGradientPoint *point)
{
    GArray *points;
    GwyGradientPoint pt;
    gint i;

    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), -1);
    g_return_val_if_fail(gradient->modifiable, -1);
    g_return_val_if_fail(point, -1);
    g_return_val_if_fail(point->x >= 0.0 && point->x <= 1.0, -1);

    pt.color = gwy_gradient_fix_rgba(&point->color);
    pt.x = point->x;

    /* find the right subinterval */
    points = gradient->points;
    for (i = 0; i < points->len; i++) {
        if (g_array_index(points, GwyGradientPoint, i).x <= pt.x)
            break;
    }
    g_assert(i < points->len);

    g_array_insert_val(points, i, pt);
    gwy_gradient_changed(gradient);

    return i;
}

/**
 * gwy_gradient_delete_point:
 * @gradient: A color gradient.
 * @index_: Color point index in @gradient.
 *
 * Deletes a point at given index in color gradient.
 *
 * It is not possible to delete points in gradients with less than 3 points.
 * First and last points should not be deleted unless there's another point
 * with @x = 0 or @x = 1 present.
 *
 * Since: 1.7
 **/
void
gwy_gradient_delete_point(GwyGradient *gradient,
                          gint index_)
{
    GArray *points;
    GwyGradientPoint *pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(gradient->points->len > 2);
    g_return_if_fail(index_ >= 0 && index_ < gradient->points->len);

    points = gradient->points;
    g_array_remove_index(points, index_);
    if (!index_) {
        pt = &g_array_index(points, GwyGradientPoint, 0);
        if (pt->x != 0.0) {
            g_warning("Forced to move first point to 0");
            pt->x = 0.0;
        }
    }
    if (index_ == points->len) {
        pt = &g_array_index(points, GwyGradientPoint, points->len-1);
        if (pt->x != 1.0) {
            g_warning("Forced to move last point to 1");
            pt->x = 1.0;
        }
    }
    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_reset:
 * @gradient: A color gradient.
 *
 * Resets a gradient to default two-point gray scale.
 *
 * Since: 1.7
 **/
void
gwy_gradient_reset(GwyGradient *gradient)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);

    g_array_set_size(gradient->points, 2);
    pt.x = 0.0;
    pt.color = black_color;
    g_array_index(gradient->points, GwyGradientPoint, 0) = pt;
    pt.x = 1.0;
    pt.color = white_color;
    g_array_index(gradient->points, GwyGradientPoint, 1) = pt;

    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_get_points:
 * @gradient: A color gradient.
 * @npoints: A location to store the number of color points (or %NULL).
 *
 * Returns the complete set of color points of a gradient.
 *
 * Returns: Complete set @gradient's color points.  The returned array is
 *          owned by @gradient and must not be modified or freed.
 *
 * Since: 1.7
 **/
const GwyGradientPoint*
gwy_gradient_get_points(GwyGradient *gradient,
                        gint *npoints)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);

    if (npoints)
        *npoints = gradient->points->len;
    return (const GwyGradientPoint*)gradient->points->data;
}

/* This is an internal function and does NOT call gwy_gradient_changed(). */
static void
gwy_gradient_sanitize(GwyGradient *gradient)
{
    GwyGradientPoint *pt;
    GArray *points;
    gint i;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);

    points = gradient->points;
    /* first make points ordered, in 0..1, starting with 0, ending with 1,
     * and fix colors */
    for (i = 0; i < points->len; i++) {
        pt = &g_array_index(points, GwyGradientPoint, i);
        pt->color = gwy_gradient_fix_rgba(&pt->color);
        if (i && pt->x < g_array_index(points, GwyGradientPoint, i-1).x) {
            g_warning("Point beyond neighbours or outside 0..1");
            pt->x = g_array_index(points, GwyGradientPoint, i-1).x;
        }
        else if (!i && pt->x != 0.0) {
            g_warning("Forced to move first point to 0");
            pt->x = 0.0;
        }
        else if (pt->x > 1.0) {
            g_warning("Point beyond neighbours or outside 0..1");
            pt->x = 1.0;
        }
    }
    pt = &g_array_index(points, GwyGradientPoint, points->len-1);
    if (pt->x != 1.0) {
        g_warning("Forced to move last point to 1");
        pt->x = 1.0;
    }

    /* then remove redundant points */
    for (i = points->len-1;
         g_array_index(points, GwyGradientPoint, i-1).x == 1.0;
         i--)
        g_array_remove_index(points, i);
    while (i) {
        pt = &g_array_index(points, GwyGradientPoint, i);
        if (pt->x == g_array_index(points, GwyGradientPoint, i-1).x
            && pt->x == g_array_index(points, GwyGradientPoint, i+1).x)
            g_array_remove_index(points, i);
        i--;
    }

    while (g_array_index(points, GwyGradientPoint, 1).x == 0.0)
        g_array_remove_index(points, 0);
}

/**
 * gwy_gradient_set_points:
 * @gradient: A color gradient.
 * @npoints: The length of @points.
 * @points: Color points to set as new gradient definition.
 *
 * Sets complete color gradient definition to given set of points.
 *
 * The point positions should be ordered, and first point should start at 0.0,
 * last end at 1.0.  There should be no redundant points.
 *
 * Since: 1.7
 **/
void
gwy_gradient_set_points(GwyGradient *gradient,
                        gint npoints,
                        const GwyGradientPoint *points)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(npoints > 1);
    g_return_if_fail(points);

    g_array_set_size(gradient->points, npoints);
    memcpy(gradient->points->data, points, npoints*sizeof(GwyGradientPoint));
    gwy_gradient_sanitize(gradient);
    gwy_gradient_changed(gradient);
}

/**
 * gwy_gradient_set_from_samples:
 * @gradient: A color gradient.
 * @nsamples: Number of samples.
 * @samples: Sampled color gradient in #GdkPixbuf-like RRGGBBAA form.
 *
 * Reconstructs color gradient definition from sampled colors.
 *
 * The result is usually approximate.
 *
 * Since: 1.7
 **/
void
gwy_gradient_set_from_samples(GwyGradient *gradient,
                              gint nsamples,
                              guchar *samples)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(gradient->modifiable);
    g_return_if_fail(samples);
    g_return_if_fail(nsamples > 0);

    /* TODO */
    g_warning("Implement me!");

    gwy_gradient_changed(gradient);
}

static void
gwy_gradient_changed(GwyGradient *gradient)
{
    /* TODO */
    gwy_debug("%s", gradient->name);
    gradient->pixels = gwy_gradient_sample(gradient,
                                           GWY_GRADIENT_DEFAULT_SIZE,
                                           gradient->pixels);
    g_signal_emit_by_name(gradient, "value_changed");
}

/**
 * gwy_gradients_gradient_exists:
 * @name: Color gradient name.
 *
 * Checks whether a color gradient exists.
 *
 * Returns: %TRUE if gradient @name exists, %FALSE if there's no such gradient.
 *
 * Since: 1.7
 **/
gboolean
gwy_gradients_gradient_exists(const gchar *name)
{
    GwyGradientClass *klass;

    g_return_val_if_fail(name, FALSE);

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, FALSE);
    return g_hash_table_lookup(klass->gradients, name) != NULL;
}

/**
 * gwy_gradients_get_gradient:
 * @name: Color gradient name.
 *
 * Returns gradient of given name.
 *
 * Returns: Color gradient @name if it exists, %NULL if there's no such
 *          gradient.  The reference count is not increased, if you want
 *          the gradient object to survive gwy_gradients_delete_gradient(),
 *          you have to add a reference yourself.
 *
 * Since: 1.7
 **/
GwyGradient*
gwy_gradients_get_gradient(const gchar *name)
{
    GwyGradientClass *klass;

    g_return_val_if_fail(name, FALSE);

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);
    return g_hash_table_lookup(klass->gradients, name);
}

/**
 * gwy_gradients_new_gradient:
 * @newname: Name of the new gradient to create.  May be %NULL, something like
 *           `Untitled N' is used then.
 *
 * Creates a new color gradient.
 *
 * The gradient is created as a two-point gray scale.
 *
 * Returns: The newly created gradient.  Its name is guaranteed to be unique
 *          and thus may differ from @newname in the case of name clash.
 *
 * Since: 1.7
 **/
GwyGradient*
gwy_gradients_new_gradient(const gchar *newname)
{
    GwyGradientClass *klass;
    gchar *realname;

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);
    realname = gwy_gradient_invent_name(klass->gradients,
                                        newname, newname != NULL);
    return gwy_gradient_new_internal(realname, 0, NULL, TRUE, TRUE);
}

/**
 * gwy_gradients_new_gradient_as_copy:
 * @name: Color gradient name.
 * @newname: Name of the new gradient to create.  May be %NULL to base the
 *           new name on @name algoritmically.
 *
 * Creates a new color gradient as a copy of an existing one.
 *
 * Returns: The newly created gradient.  Its name is guaranteed to be unique
 *          and thus may differ from @newname in the case of name clash.
 *
 * Since: 1.7
 **/
GwyGradient*
gwy_gradients_new_gradient_as_copy(const gchar *name,
                                   const gchar *newname)
{
    GwyGradientClass *klass;
    GwyGradient *gradient;
    gchar *realname;

    g_return_val_if_fail(name, FALSE);

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);
    gradient = (GwyGradient*)g_hash_table_lookup(klass->gradients, name);
    g_return_val_if_fail(gradient, NULL);

    realname = gwy_gradient_invent_name(klass->gradients,
                                        newname ? newname : name,
                                        newname != NULL);
    return gwy_gradient_new_internal(realname, gradient->points->len,
                                     (GwyGradientPoint*)gradient->points,
                                     TRUE, TRUE);
}

static gchar*
gwy_gradient_invent_name(GHashTable *gradients,
                         const gchar *prefix,
                         gboolean warn)
{
    gchar *str, *p;
    gint n, i;

    if (!prefix)
        prefix = _("Untitled");
    if (!g_hash_table_lookup(gradients, prefix))
        return g_strdup(prefix);
    if (warn)
        g_warning("Gradient name clash");

    /* remove eventual trailing digits before appending new */
    p = strrchr(prefix, ' ');
    n = strlen(prefix);
    if (p && strspn(p+1, "0123456789") == strlen(p+1)) {
        n = p - prefix;
    }

    str = g_new(gchar, n + 10);
    strncpy(str, prefix, n);
    str[n] = ' ';
    for (i = 1; i < 1000000; i++) {
        g_snprintf(str + n + 1, 9, "%d", i);
        if (!g_hash_table_lookup(gradients, str))
            return str;
    }
    g_return_val_if_reached(NULL);
}

/**
 * gwy_gradients_delete_gradient:
 * @name: Color gradient name.
 *
 * Deletes a color gradient.
 *
 * Gradient objects of deleted gradients that are referenced elsewhere will
 * continue to exist until all references to them are dropped, but they can
 * be no longer obtained with gwy_gradients_get() and their name can be
 * reused.  In other words, gradient deletion works like file deletion on Unix.
 *
 * Returns: %TRUE if there was such a gradient and was deleted.
 *
 * Since: 1.7
 **/
gboolean
gwy_gradients_delete_gradient(const gchar *name)
{
    GwyGradientClass *klass;
    GwyGradient *gradient;

    g_return_val_if_fail(name, FALSE);

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, FALSE);
    gradient = (GwyGradient*)g_hash_table_lookup(klass->gradients, name);
    if (!gradient)
        return FALSE;

    g_return_val_if_fail(gradient->modifiable, FALSE);
    return g_hash_table_remove(klass->gradients, name);
}

/**
 * gwy_gradients_rename_gradient:
 * @name: Color gradient name.
 * @newname: Name to rename gradient to.
 *
 * Renames a color gradient.
 *
 * It is an error to try to rename a non-modifiable (system) gradient or to
 * rename a gradient to an already existing name.
 *
 * Returns: The renamed gradient, for convenience.
 *
 * Since: 1.7
 **/
GwyGradient*
gwy_gradients_rename_gradient(const gchar *name,
                              const gchar *newname)
{
    GwyGradientClass *klass;
    GwyGradient *gradient, *newgradient;

    g_return_val_if_fail(name, NULL);
    g_return_val_if_fail(newname, NULL);

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);
    gradient = (GwyGradient*)g_hash_table_lookup(klass->gradients, name);
    g_return_val_if_fail(gradient, NULL);
    g_return_val_if_fail(gradient->modifiable, NULL);
    newgradient = (GwyGradient*)g_hash_table_lookup(klass->gradients, newname);
    g_return_val_if_fail(!newgradient, NULL);

    g_free(gradient->name);
    gradient->name = g_strdup(newname);
    /* FIXME: should emit some signal(?) */

    return gradient;
}

/**
 * gwy_gradients_foreach:
 * @function: Function to call on each color gradient.
 * @user_data: Data to pass as @user_data to @function.
 *
 * Calls a function for each color gradient.
 *
 * Since: 1.7
 **/
void
gwy_gradients_foreach(GwyGradientFunc function,
                      gpointer user_data)
{
    GwyGradientClass *klass;

    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_if_fail(klass);
    g_hash_table_foreach(klass->gradients, (GHFunc)function, user_data);
}

static inline void
gwy_gradient_preset(const gchar *name,
                    gint npoints,
                    const GwyGradientPoint *points)
{
    gwy_gradient_new_internal(g_strdup(name), npoints, points, FALSE, TRUE);
}

/**
 * gwy_gradients_setup_presets:
 *
 * Sets up built-in color gradients.
 *
 * Should be done during program initialization if built-in gradients are to
 * be used, before user gradients are loaded.
 *
 * Preset (system) gradients are not modifiable.
 *
 * Since: 1.7
 **/
void
gwy_gradients_setup_presets(void)
{
    static const GwyRGBA xyellow = { 0.8314, 0.71765, 0.16471, 1 };
    static const GwyRGBA pink = { 1, 0.07843, 0.62745, 1 };
    static const GwyRGBA olive = { 0.36863, 0.69020, 0.45882, 1 };
    static const GwyGradientPoint gray[] = {
        { 0.0, { 0, 0, 0, 1 } },
        { 1.0, { 1, 1, 1, 1 } },
    };
    static const GwyGradientPoint rainbow1[] = {
        { 0.0,   { 0, 0, 0, 1 } },
        { 0.125, { 1, 0, 0, 1 } },
        { 0.25,  { 1, 1, 0, 1 } },
        { 0.375, { 0, 1, 1, 1 } },
        { 0.5,   { 1, 0, 1, 1 } },
        { 0.625, { 0, 1, 0, 1 } },
        { 0.75,  { 0, 0, 1, 1 } },
        { 0.875, { 0.5, 0.5, 0.5, 1 } },
        { 1.0,   { 1, 1, 1, 1 } },
    };
    static const GwyGradientPoint rainbow2[] = {
        { 0.0,  { 0, 0, 0, 1 } },
        { 0.25, { 1, 0, 0, 1 } },
        { 0.5,  { 0, 1, 0, 1 } },
        { 0.75, { 0, 0, 1, 1 } },
        { 1.0,  { 1, 1, 1, 1 } },
    };
    static const GwyGradientPoint gold[] = {
        { 0,        { 0, 0, 0, 1 } },
        { 0.333333, { 0.345098, 0.109804, 0, 1 } },
        { 0.666667, { 0.737255, 0.501961, 0, 1 } },
        { 1,        { 0.988235, 0.988235, 0.501961, 1 } },
    };
    static const GwyGradientPoint pm3d[] = {
        { 0,        { 0,        0,        0,        1 } },
        { 0.166667, { 0.265412, 0,        0.564000, 1 } },
        { 0.333333, { 0.391234, 0,        0.831373, 1 } },
        { 0.666667, { 0.764706, 0,        0.000000, 1 } },
        { 1,        { 1.000000, 0.894118, 0.000000, 1 } },
    };
    static const GwyGradientPoint spectral[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1 } },
        { 0.090909, { 0.885000, 0.024681, 0.017629, 1 } },
        { 0.181818, { 1.000000, 0.541833, 0.015936, 1 } },
        { 0.272727, { 0.992157, 0.952941, 0.015686, 1 } },
        { 0.363636, { 0.511640, 0.833000, 0.173365, 1 } },
        { 0.454545, { 0.243246, 0.705000, 0.251491, 1 } },
        { 0.545455, { 0.332048, 0.775843, 0.795000, 1 } },
        { 0.636364, { 0.019608, 0.529412, 0.819608, 1 } },
        { 0.727273, { 0.015686, 0.047059, 0.619608, 1 } },
        { 0.818182, { 0.388235, 0.007843, 0.678431, 1 } },
        { 0.909091, { 0.533279, 0.008162, 0.536000, 1 } },
        { 1.000000, { 0.000000, 0.000000, 0.000000, 1 } },
    };
    static const GwyGradientPoint warm[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1 } },
        { 0.250000, { 0.484848, 0.188417, 0.266572, 1 } },
        { 0.450000, { 0.760000, 0.182400, 0.182400, 1 } },
        { 0.600000, { 0.870000, 0.495587, 0.113100, 1 } },
        { 0.750000, { 0.890000, 0.751788, 0.106800, 1 } },
        { 0.900000, { 0.909090, 0.909091, 0.909090, 1 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1 } },
    };
    static const GwyGradientPoint cold[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1 } },
        { 0.300000, { 0.168223, 0.273350, 0.488636, 1 } },
        { 0.500000, { 0.196294, 0.404327, 0.606061, 1 } },
        { 0.700000, { 0.338800, 0.673882, 0.770000, 1 } },
        { 0.900000, { 0.909090, 0.909091, 0.909090, 1 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1 } },
    };
    static const GwyGradientPoint dfit[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1 } },
        { 0.076923, { 0.435640, 0.135294, 0.500000, 1 } },
        { 0.153846, { 0.871280, 0.270588, 1.000000, 1 } },
        { 0.230769, { 0.935640, 0.270588, 0.729688, 1 } },
        { 0.307692, { 1.000000, 0.270588, 0.459377, 1 } },
        { 0.384615, { 1.000000, 0.570934, 0.364982, 1 } },
        { 0.461538, { 1.000000, 0.871280, 0.270588, 1 } },
        { 0.538461, { 0.601604, 0.906715, 0.341219, 1 } },
        { 0.615384, { 0.203209, 0.942149, 0.411850, 1 } },
        { 0.692307, { 0.207756, 0.695298, 0.698082, 1 } },
        { 0.769230, { 0.212303, 0.448447, 0.984314, 1 } },
        { 0.846153, { 0.561152, 0.679224, 0.947157, 1 } },
        { 0.923076, { 0.909090, 0.909091, 0.909090, 1 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1 } },
    };
    static const GwyGradientPoint spring[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1.000000 } },
        { 0.250000, { 0.059669, 0.380392, 0.293608, 1.000000 } },
        { 0.500000, { 0.084395, 0.650980, 0.025529, 1.000000 } },
        { 0.750000, { 0.758756, 0.850980, 0.560646, 1.000000 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1.000000 } },
    };
    static const GwyGradientPoint body[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1.000000 } },
        { 0.200000, { 0.492424, 0.303700, 0.136994, 1.000000 } },
        { 0.400000, { 0.749020, 0.280947, 0.117493, 1.000000 } },
        { 0.600000, { 0.880909, 0.563001, 0.482738, 1.000000 } },
        { 0.800000, { 1.000000, 0.855548, 0.603922, 1.000000 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1.000000 } },
    };
    static const GwyGradientPoint sky[] = {
        { 0.000000, { 0.000000, 0.000000, 0.000000, 1.000000 } },
        { 0.200000, { 0.149112, 0.160734, 0.396078, 1.000000 } },
        { 0.400000, { 0.294641, 0.391785, 0.466667, 1.000000 } },
        { 0.600000, { 0.792157, 0.476975, 0.245413, 1.000000 } },
        { 0.800000, { 0.988235, 0.826425, 0.333287, 1.000000 } },
        { 1.000000, { 1.000000, 1.000000, 1.000000, 1.000000 } },
    };
    static const GwyGradientPoint lines[] = {
        { 0.000, { 1.0, 1.0, 0.0, 1 } },
        { 0.006, { 1.0, 1.0, 0.0, 1 } },
        { 0.007, { 0.0, 0.0, 0.0, 1 } },
        { 0.195, { 0.2, 0.2, 0.2, 1 } },
        { 0.196, { 0.0, 1.0, 1.0, 1 } },
        { 0.204, { 0.0, 1.0, 1.0, 1 } },
        { 0.205, { 0.2, 0.2, 0.2, 1 } },
        { 0.395, { 0.4, 0.4, 0.4, 1 } },
        { 0.396, { 0.0, 1.0, 0.0, 1 } },
        { 0.404, { 0.0, 1.0, 0.0, 1 } },
        { 0.405, { 0.4, 0.4, 0.4, 1 } },
        { 0.595, { 0.6, 0.6, 0.6, 1 } },
        { 0.596, { 1.0, 0.0, 1.0, 1 } },
        { 0.604, { 1.0, 0.0, 1.0, 1 } },
        { 0.605, { 0.6, 0.6, 0.6, 1 } },
        { 0.795, { 0.8, 0.8, 0.8, 1 } },
        { 0.796, { 1.0, 0.0, 0.0, 1 } },
        { 0.804, { 1.0, 0.0, 0.0, 1 } },
        { 0.805, { 0.8, 0.8, 0.8, 1 } },
        { 0.993, { 1.0, 1.0, 1.0, 1 } },
        { 0.994, { 0.0, 0.0, 1.0, 1 } },
        { 1.000, { 0.0, 0.0, 1.0, 1 } },
    };

    static GwyGradientPoint pd[] = {
        { 0.0, { 0, 0, 0, 1 } },
        { 0.5, { 0, 0, 0, 0 } },
        { 1.0, { 1, 1, 1, 1 } },
    };
    static GwyGradientPoint pd3[] = {
        { 0.0,  { 0, 0, 0, 1 } },
        { 0.33, { 0, 0, 0, 0 } },
        { 0.67, { 0, 0, 0, 0 } },
        { 1.0,  { 1, 1, 1, 1 } },
    };
    static GwyGradientPoint pd4[] = {
        { 0.0,  { 0,   0,   0,   1 } },
        { 0.33, { 0,   0,   0,   0 } },
        { 0.5,  { .67, .67, .67, 1 } },
        { 0.67, { 0,   0,   0,   0 } },
        { 1.0,  { 1,   1,   1,   1 } },
    };
    GwyGradientPoint *pd2;
    guint i;

    gwy_gradient_preset(GWY_PALETTE_GRAY, G_N_ELEMENTS(gray), gray);
    gwy_gradient_preset(GWY_PALETTE_RAINBOW1, G_N_ELEMENTS(rainbow1), rainbow1),
    gwy_gradient_preset(GWY_PALETTE_RAINBOW2, G_N_ELEMENTS(rainbow2), rainbow2);
    gwy_gradient_preset(GWY_PALETTE_GOLD, G_N_ELEMENTS(gold), gold);
    gwy_gradient_preset(GWY_PALETTE_PM3D, G_N_ELEMENTS(pm3d), pm3d);
    gwy_gradient_preset(GWY_PALETTE_SPECTRAL, G_N_ELEMENTS(spectral), spectral);
    gwy_gradient_preset(GWY_PALETTE_WARM, G_N_ELEMENTS(warm), warm);
    gwy_gradient_preset(GWY_PALETTE_COLD, G_N_ELEMENTS(cold), cold);
    gwy_gradient_preset(GWY_PALETTE_DFIT, G_N_ELEMENTS(dfit), dfit);

    gwy_gradient_preset("Spring", G_N_ELEMENTS(spring), spring);
    gwy_gradient_preset("Body", G_N_ELEMENTS(body), body);
    gwy_gradient_preset("Sky", G_N_ELEMENTS(sky), sky);
    gwy_gradient_preset("Lines", G_N_ELEMENTS(lines), lines);

    pd[1].color = red_color;
    gwy_gradient_preset(GWY_PALETTE_RED, G_N_ELEMENTS(pd), pd);
    pd[1].color = green_color;
    gwy_gradient_preset(GWY_PALETTE_GREEN, G_N_ELEMENTS(pd), pd);
    pd[1].color = blue_color;
    gwy_gradient_preset(GWY_PALETTE_BLUE, G_N_ELEMENTS(pd), pd);
    pd[1].color = xyellow;
    gwy_gradient_preset(GWY_PALETTE_YELLOW, G_N_ELEMENTS(pd), pd);
    pd[1].color = pink;
    gwy_gradient_preset(GWY_PALETTE_PINK, G_N_ELEMENTS(pd), pd);
    pd[1].color = olive;
    gwy_gradient_preset(GWY_PALETTE_OLIVE, G_N_ELEMENTS(pd), pd);

    pd3[1].color = red_color;
    pd3[2].color = yellow_color;
    gwy_gradient_preset(GWY_PALETTE_RED_YELLOW, G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = violet_color;
    gwy_gradient_preset(GWY_PALETTE_RED_VIOLET, G_N_ELEMENTS(pd3), pd3);

    pd3[1].color = blue_color;
    pd3[2].color = cyan_color;
    gwy_gradient_preset(GWY_PALETTE_BLUE_CYAN, G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = violet_color;
    gwy_gradient_preset(GWY_PALETTE_BLUE_VIOLET, G_N_ELEMENTS(pd3), pd3);

    pd3[1].color = green_color;
    pd3[2].color = yellow_color;
    gwy_gradient_preset(GWY_PALETTE_GREEN_YELLOW, G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = cyan_color;
    gwy_gradient_preset(GWY_PALETTE_GREEN_CYAN, G_N_ELEMENTS(pd3), pd3);

    pd4[1].color = red_color;
    pd4[3].color = cyan_color;
    gwy_gradient_preset(GWY_PALETTE_RED_CYAN, G_N_ELEMENTS(pd4), pd4);
    pd4[1].color = blue_color;
    pd4[3].color = yellow_color;
    gwy_gradient_preset(GWY_PALETTE_BLUE_YELLOW, G_N_ELEMENTS(pd4), pd4);
    pd4[1].color = green_color;
    pd4[3].color = violet_color;
    gwy_gradient_preset(GWY_PALETTE_GREEN_VIOLET, G_N_ELEMENTS(pd4), pd4);

    pd2 = g_new(GwyGradientPoint, 20);
    for (i = 0; i < 10; i++) {
        pd2[i].x = i/9.0;
        pd2[i].color = i%2 ? black_color : white_color;
    }
    gwy_gradient_preset(GWY_PALETTE_BW1, 10, pd2);

    pd2[0].x = 0.0;
    pd2[0].color = black_color;
    for (i = 1; i < 19; i++) {
        pd2[i].x = (i/2 + i%2)/10.0 + (i%2 ? -0.01 : 0.01);
        pd2[i].color = i/2%2 ? white_color : black_color;
    }
    pd2[19].x = 1.0;
    pd2[19].color = white_color;
    gwy_gradient_preset(GWY_PALETTE_BW2, 20, pd2);
    g_free(pd2);
}

/* Eats @name */
static GwyGradient*
gwy_gradient_new_internal(gchar *name,
                          gint npoints,
                          const GwyGradientPoint *points,
                          gboolean modifiable,
                          gboolean do_sample)
{
    GwyGradientClass *klass;
    GwyGradient *gradient;

    g_return_val_if_fail(name, NULL);

    gradient = g_object_new(GWY_TYPE_GRADIENT, NULL);
    klass = (GwyGradientClass*)g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);
    if (g_hash_table_lookup(klass->gradients, name)) {
        g_critical("Gradient <%s> already exists", name);
        g_object_unref(gradient);
        gradient = (GwyGradient*)g_hash_table_lookup(klass->gradients, name);
        g_object_ref(gradient);

        return gradient;
    }

    gradient->name = name;
    gradient->modifiable = modifiable;
    if (npoints && points) {
        gradient->points = g_array_sized_new(FALSE, FALSE,
                                             sizeof(GwyGradientPoint), npoints);
        g_array_append_vals(gradient->points, points, npoints);
    }
    else {
        GwyGradientPoint pt;

        gradient->points = g_array_sized_new(FALSE, FALSE,
                                             sizeof(GwyGradientPoint), 2);
        pt.x = 0.0;
        pt.color = black_color;
        g_array_append_val(gradient->points, pt);
        pt.x = 1.0;
        pt.color = white_color;
        g_array_append_val(gradient->points, pt);
    }

    g_hash_table_insert(klass->gradients, gradient->name, gradient);
    if (do_sample)
       gradient->pixels = gwy_gradient_sample(gradient,
                                              GWY_GRADIENT_DEFAULT_SIZE, NULL);

    return gradient;
}

static GByteArray*
gwy_gradient_serialize(GObject *obj,
                       GByteArray *buffer)
{
    GwyGradient *gradient;
    GwyGradientPoint *pt;
    GArray *points;
    gdouble *data, *rdata, *gdata, *bdata, *adata, *xdata;
    gsize ndata, i;

    g_return_val_if_fail(GWY_IS_GRADIENT(obj), NULL);

    gradient = GWY_GRADIENT(obj);
    points = gradient->points;
    ndata = points->len;
    data = g_new(gdouble, 5*ndata);
    xdata = data;
    rdata = data + ndata;
    gdata = data + 2*ndata;
    bdata = data + 3*ndata;
    adata = data + 4*ndata;

    for (i = 0; i < ndata; i++) {
        pt = &g_array_index(points, GwyGradientPoint, i);
        xdata[i] = pt->x;
        rdata[i] = pt->color.r;
        gdata[i] = pt->color.g;
        bdata[i] = pt->color.b;
        adata[i] = pt->color.a;
    }

    {
        GwySerializeSpec specs[] = {
            { 's', "name",  &gradient->name, NULL,  },
            { 'D', "x",     &xdata,          &ndata, },
            { 'D', "red",   &rdata,          &ndata, },
            { 'D', "green", &gdata,          &ndata, },
            { 'D', "blue",  &bdata,          &ndata, },
            { 'D', "alpha", &adata,          &ndata, },
        };
        buffer = gwy_serialize_pack_object_struct(buffer,
                                                  GWY_GRADIENT_TYPE_NAME,
                                                  G_N_ELEMENTS(specs), specs);
    }
    g_free(data);

    return buffer;
}

static GObject*
gwy_gradient_deserialize(const guchar *buffer,
                         gsize size,
                         gsize *position)
{
    gint nrdata, ngdata, nbdata, nadata, nxdata, i;
    GArray *points;
    GwyGradient *gradient;
    GwyGradientPoint pt;
    gdouble *rdata = NULL, *gdata = NULL, *bdata = NULL,
            *adata = NULL, *xdata = NULL;
    gchar *name = NULL;
    GwySerializeSpec specs[] = {
        { 's', "name",  &name,  NULL,    },
        { 'D', "x",     &xdata, &nxdata, },
        { 'D', "red",   &rdata, &nrdata, },
        { 'D', "green", &gdata, &ngdata, },
        { 'D', "blue",  &bdata, &nbdata, },
        { 'D', "alpha", &adata, &nadata, },
    };

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_GRADIENT_TYPE_NAME,
                                            G_N_ELEMENTS(specs), specs))
        goto fail;
    if (!name) {
        g_critical("Gradient has no name");
        goto fail;
    }
    if (nxdata != nrdata
        || nxdata != ngdata
        || nxdata != nbdata
        || nxdata != nadata) {
        g_critical("Gradient %s array component lengths differ", name);
        goto fail;
    }
    if (nxdata < 2) {
        g_critical("Gradient %s has too few points", name);
        goto fail;
    }

    gradient = gwy_gradients_get_gradient(name);
    if (gradient) {
        if (!gradient->modifiable) {
            g_critical("Cannot overwrite system gradient %s", name);
            goto fail;
        }
        g_warning("Deserializing existing gradient %s", name);
        g_free(name);
    }
    else
        gradient = gwy_gradient_new_internal(name, 0, NULL, TRUE, FALSE);

    points = gradient->points;
    g_array_set_size(points, 0);
    for (i = 0; i < nxdata; i++) {
        pt.x = xdata[i];
        pt.color.r = rdata[i];
        pt.color.g = gdata[i];
        pt.color.b = bdata[i];
        pt.color.a = adata[i];
        g_array_append_val(points, pt);
    }
    g_free(rdata);
    g_free(gdata);
    g_free(bdata);
    g_free(adata);
    g_free(xdata);

    gwy_gradient_sanitize(gradient);
    gwy_gradient_changed(gradient);

    return (GObject*)gradient;

fail:
    g_free(rdata);
    g_free(gdata);
    g_free(bdata);
    g_free(adata);
    g_free(xdata);
    g_free(name);
    return NULL;
}

static GObject*
gwy_gradient_duplicate(GObject *object)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(object), NULL);
    g_object_ref(object);
    return object;
}

/**
 * gwy_gradient_dump:
 * @gradient: A color gradient.
 *
 * Dumps text a color gradient definition.
 *
 * Returns: A #GString with gradient text representation.
 *
 * Since: 1.7
 **/
GString*
gwy_gradient_dump(GwyGradient *gradient)
{
    GwyGradientPoint *pt;
    GString *str;
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    guint i;

    g_return_val_if_fail(GWY_IS_PALETTE_DEF(gradient), NULL);
    g_return_val_if_fail(gradient->points->len > 0, NULL);

    str = g_string_sized_new(64*gradient->points->len);
    g_string_append(str, magic_header);
    g_string_append_c(str, '\n');
    g_string_append_printf(str, "name=%s\n", gradient->name);
    g_string_append(str, "data\n");
    for (i = 0; i < gradient->points->len; i++) {
        pt = &g_array_index(gradient->points, GwyGradientPoint, i);
        /* this is ugly.  I hate locales */
        g_ascii_dtostr(buffer, sizeof(buffer), pt->x);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_dtostr(buffer, sizeof(buffer), pt->color.r);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_dtostr(buffer, sizeof(buffer), pt->color.g);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_dtostr(buffer, sizeof(buffer), pt->color.b);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_dtostr(buffer, sizeof(buffer), pt->color.a);
        g_string_append(str, buffer);
        g_string_append_c(str, '\n');
    }

    return str;
}

/**
 * gwy_gradient_parse:
 * @text: A color gradient definition dump, as created with
 *        gwy_gradient_dump().
 *
 * Creates a color gradient from a text dump.
 *
 * This function fails if the gradient already exists.
 *
 * Returns: The reconstructed gradient.
 *
 * Since: 1.7
 **/
GwyGradient*
gwy_gradient_parse(const gchar *text)
{
    GwyGradient *gradient = NULL;
    GwyGradientClass *klass;
    GArray *points = NULL;
    GwyGradientPoint pt;
    gchar *name = NULL;
    gchar *str, *p, *line, *end;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_PALETTE_DEF);
    g_return_val_if_fail(klass, NULL);

    p = str = g_strdup(text);

    if (!(line = gwy_str_next_line(&p)) || strcmp(line, magic_header)) {
        g_warning("Wrong magic header");
        goto fail;
    }

    if (!(line = gwy_str_next_line(&p)) || !g_str_has_prefix(line, "name=")) {
        g_warning("Expected gradient name");
        goto fail;
    }
    name = g_strdup(g_strstrip(line + sizeof("name=")));
    if (!*name) {
        g_warning("Bad gradient name");
        goto fail;
    }
    if (g_hash_table_lookup(klass->gradients, name)) {
        g_warning("Gradient `%s' already exists", name);
        goto fail;
    }

    if (!(line = gwy_str_next_line(&p)) || strcmp(line, "data")) {
        g_warning("Expected gradient `%s' data", name);
        goto fail;
    }

    points = g_array_new(FALSE, FALSE, sizeof(GwyGradientPoint));
    while (TRUE) {
        if (!(line = gwy_str_next_line(&p)))
            break;
        /* this is ugly.  I hate locales */
        pt.x = g_ascii_strtod(line, &end);
        if (end == line)
            break;
        line = end;
        pt.color.r = g_ascii_strtod(line, &end);
        if (end == line)
            break;
        line = end;
        pt.color.g = g_ascii_strtod(line, &end);
        if (end == line)
            break;
        line = end;
        pt.color.b = g_ascii_strtod(line, &end);
        if (end == line)
            break;
        line = end;
        pt.color.a = g_ascii_strtod(line, &end);
        if (end == line)
            break;

        g_array_append_val(points, pt);
    }
    if (line) {
        g_warning("Cannot parse color point in `%s'", name);
        goto fail;
    }

    gradient = gwy_gradient_new_internal(name, 0, NULL, TRUE, FALSE);
    gwy_gradient_set_points(gradient,
                            points->len, (GwyGradientPoint*)points->data);
    name = NULL;

fail:
    if (points)
        g_array_free(points, TRUE);
    g_free(name);
    g_free(str);
    return gradient;
}


/************************** Documentation ****************************/

/**
 * GwyGradient:
 *
 * The #GwyGradient struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
