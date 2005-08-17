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
 * - reduce the number of system palettes -- in 2.0
 * - set from samples
 * - test
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwygradient.h"

#define GWY_GRADIENT_TYPE_NAME "GwyGradient"

#define GWY_GRADIENT_DEFAULT "Gray"

/* Standard sample size, returned by gwy_gradient_get_samples() */
enum {
    GWY_GRADIENT_DEFAULT_SIZE = 1024
};

#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void         gwy_gradient_finalize         (GObject *object);
static gpointer     gwy_gradient_copy             (gpointer);
static const GType* gwy_gradient_get_traits       (gint *ntraits);
static const gchar* gwy_gradient_get_trait_name   (gint i);
static void         gwy_gradient_get_trait_value  (gpointer item,
                                                   gint i,
                                                   GValue *value);
static void         gwy_gradient_use              (GwyResource *resource);
static void         gwy_gradient_release          (GwyResource *resource);
static void         gwy_gradient_sample_real      (GwyGradient *gradient,
                                                   gint nsamples,
                                                   guchar *samples);
static void         gwy_gradient_sanitize         (GwyGradient *gradient);
static void         gwy_gradient_changed          (GwyGradient *gradient);
static void         gwy_gradient_preset           (const gchar *name,
                                                   gint npoints,
                                                   const GwyGradientPoint *points);
static GwyGradient* gwy_gradient_new              (const gchar *name,
                                                   gint npoints,
                                                   const GwyGradientPoint *points);
static void         gwy_gradient_dump             (GwyResource *resource,
                                                   GString *str);
static GwyResource* gwy_gradient_parse            (const gchar *text);


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

G_DEFINE_TYPE(GwyGradient, gwy_gradient, GWY_TYPE_RESOURCE)

static void
gwy_gradient_class_init(GwyGradientClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_gradient_finalize;

    parent_class = GWY_RESOURCE_CLASS(gwy_gradient_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->item_type.copy = gwy_gradient_copy;
    res_class->item_type.get_traits = gwy_gradient_get_traits;
    res_class->item_type.get_trait_name = gwy_gradient_get_trait_name;
    res_class->item_type.get_trait_value = gwy_gradient_get_trait_value;

    res_class->name = "gradient";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_set_default_item_name(res_class->inventory,
                                        GWY_GRADIENT_DEFAULT);
    res_class->use = gwy_gradient_use;
    res_class->release = gwy_gradient_release;
    res_class->dump = gwy_gradient_dump;
    res_class->parse = gwy_gradient_parse;
}

static void
gwy_gradient_init(GwyGradient *gradient)
{
    GwyGradientPoint pt;

    gwy_debug_objects_creation(G_OBJECT(gradient));

    gradient->points = g_array_sized_new(FALSE, FALSE,
                                         sizeof(GwyGradientPoint), 2);
    pt.x = 0.0;
    pt.color = black_color;
    g_array_index(gradient->points, GwyGradientPoint, 0) = pt;
    pt.x = 1.0;
    pt.color = white_color;
    g_array_index(gradient->points, GwyGradientPoint, 1) = pt;
}

static void
gwy_gradient_finalize(GObject *object)
{
    GwyGradient *gradient = (GwyGradient*)object;

    g_array_free(gradient->points, TRUE);
    G_OBJECT_CLASS(gwy_gradient_parent_class)->finalize(object);
}

/* FIXME: This looks too much like GObject properties.
 * Define them as properties and use a generic property -> trait mapping? */
static const GType*
gwy_gradient_get_traits(gint *ntraits)
{
    static GType tratis[] = { G_TYPE_STRING, G_TYPE_BOOLEAN };

    if (ntraits)
        *ntraits = G_N_ELEMENTS(tratis);

    return tratis;
}

static const gchar*
gwy_gradient_get_trait_name(gint i)
{
    static const gchar *trait_names[] = { "name", "is-const" };

    g_return_val_if_fail(i >= 0 && i < G_N_ELEMENTS(trait_names), NULL);
    return trait_names[i];
}

static void
gwy_gradient_get_trait_value(gpointer item,
                             gint i,
                             GValue *value)
{
    switch (i) {
        case 0:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_object(value, GWY_RESOURCE(item)->name);
        break;

        case 1:
        g_value_init(value, G_TYPE_BOOLEAN);
        g_value_set_boolean(value, GWY_RESOURCE(item)->is_const);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_gradient_use(GwyResource *resource)
{
    GwyGradient *gradient;

    gradient = GWY_GRADIENT(resource);
    g_assert(gradient->pixels == NULL);
    gradient->pixels = gwy_gradient_sample(gradient, GWY_GRADIENT_DEFAULT_SIZE,
                                           NULL);
}

static void
gwy_gradient_release(GwyResource *resource)
{
    GwyGradient *gradient;

    gradient = GWY_GRADIENT(resource);
    g_free(gradient->pixels);
    gradient->pixels = NULL;
}

/**
 * gwy_gradient_get_color:
 * @gradient: A color gradient.
 * @x: Position in gradient, in range 0..1.
 * @color: Color to fill with interpolated color at position @x.
 *
 * Computes color at given position of a color gradient.
 **/
void
gwy_gradient_get_color(GwyGradient *gradient,
                       gdouble x,
                       GwyRGBA *color)
{
    GArray *points;
    GwyGradientPoint *pt = NULL, *pt2;
    guint i;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(color);
    g_return_if_fail(x >= 0.0 && x <= 1.0);

    points = gradient->points;

    /* find the right subinterval */
    for (i = 0; i < points->len; i++) {
        pt = &g_array_index(points, GwyGradientPoint, i);
        if (pt->x == x) {
            *color = pt->color;
            return;
        }
        if (pt->x > x)
            break;
    }
    g_assert(i);
    pt2 = &g_array_index(points, GwyGradientPoint, i-1);

    gwy_rgba_interpolate(&pt2->color, &pt->color, (x - pt2->x)/(pt->x - pt2->x),
                         color);
}

/**
 * gwy_gradient_get_samples:
 * @gradient: A color gradient to get samples of.
 * @nsamples: A location to store the number of samples (or %NULL).
 *
 * Returns color gradient sampled to integers in #GdkPixbuf-like scheme.
 *
 * The returned samples are owned by @gradient and must not be modified or
 * freed.  They are automatically updated when the gradient changes, although
 * their number never changes.  The returned pointer is valid only as long
 * as the gradient used, indicated by gwy_resource_use().
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 **/
const guchar*
gwy_gradient_get_samples(GwyGradient *gradient,
                         gint *nsamples)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    if (!GWY_RESOURCE(gradient)->use_count) {
        g_warning("You have to call gwy_resource_use() first. "
                  "I'll try to be nice and do that for this once.");
        gwy_resource_use(GWY_RESOURCE(gradient));
    }
    if (nsamples)
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
 * This function does not need the gradient to be in use, though.
 *
 * Returns: Sampled @gradient as a sequence of #GdkPixbuf-like RRGGBBAA
 *          quadruplets.
 **/
guchar*
gwy_gradient_sample(GwyGradient *gradient,
                    gint nsamples,
                    guchar *samples)
{
    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), NULL);
    g_return_val_if_fail(nsamples > 1, NULL);

    samples = g_renew(guchar, samples, 4*nsamples);
    gwy_gradient_sample_real(gradient, nsamples, samples);

    return samples;
}

static void
gwy_gradient_sample_real(GwyGradient *gradient,
                         gint nsamples,
                         guchar *samples)
{
    GwyGradientPoint *pt, *pt2 = NULL;
    gint i, j, k;
    gdouble q, x;
    GwyRGBA color;

    q = 1.0/(nsamples - 1.0);
    pt = &g_array_index(gradient->points, GwyGradientPoint, 0);
    for (i = j = k = 0; i < nsamples; i++) {
        x = i*q;
        while (G_UNLIKELY(x > pt->x)) {
            j++;
            pt2 = pt;
            pt = &g_array_index(gradient->points, GwyGradientPoint, j);
        }
        if (G_UNLIKELY(x == pt->x))
            color = pt->color;
        else
            gwy_rgba_interpolate(&pt2->color, &pt->color,
                                 (x - pt2->x)/(pt->x - pt2->x),
                                 &color);

        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.r);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.g);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.b);
        samples[k++] = (guchar)(gint32)(MAX_CVAL*color.a);
    }
}

/**
 * gwy_gradient_sample_to_pixbuf:
 * @gradient: A color gradient to sample.
 * @pixbuf: A pixbuf to sample gradient to (in horizontal direction).
 *
 * Samples gradient to a provided pixbuf.
 *
 * Unlike gwy_gradient_sample() which simply takes samples at equidistant
 * points this method uses supersampling and thus gives a bit better looking
 * gradient presentation.
 **/
void
gwy_gradient_sample_to_pixbuf(GwyGradient *gradient,
                              GdkPixbuf *pixbuf)
{
    /* Supersample to capture abrupt changes and peaks more faithfully.
     * Note an even number would lead to biased integer averaging. */
    enum { SUPERSAMPLE = 3 };
    gint width, height, rowstride, i, j;
    gboolean has_alpha, must_free_data;
    guchar *data, *pdata;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pdata = gdk_pixbuf_get_pixels(pixbuf);

    /* Usually the pixbuf is large enough to be used as a scratch space,
     * there is no need to allocate extra memory then. */
    if ((must_free_data = (SUPERSAMPLE*width*4 > rowstride*height)))
        data = g_new(guchar, SUPERSAMPLE*width*4);
    else
        data = pdata;

    gwy_gradient_sample_real(gradient, SUPERSAMPLE*width, data);

    /* Scale down to original size */
    for (i = 0; i < width; i++) {
        guchar *row = data + 4*SUPERSAMPLE*i;
        guint r, g, b, a;

        r = g = b = a = SUPERSAMPLE/2;
        for (j = 0; j < SUPERSAMPLE; j++) {
            r += *(row++);
            g += *(row++);
            b += *(row++);
            a += *(row++);
        }
        *(pdata++) = r/SUPERSAMPLE;
        *(pdata++) = g/SUPERSAMPLE;
        *(pdata++) = b/SUPERSAMPLE;
        if (has_alpha)
            *(pdata++) = a/SUPERSAMPLE;
    }

    /* Duplicate rows */
    pdata = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 0; i < height; i++)
        memcpy(pdata + i*rowstride, pdata, rowstride);

    if (must_free_data)
        g_free(data);
}

/**
 * gwy_gradient_get_npoints:
 * @gradient: A color gradient.
 *
 * Returns the number of points in a gradient.
 *
 * Returns: The number of points in @gradient.
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

/**
 * gwy_gradient_fix_rgba:
 * @color: A color.
 *
 * Fixes color components to range 0..1.
 *
 * Returns: The fixed color.
 **/
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

/**
 * gwy_gradient_fix_position:
 * @points: Array of color points (gradient definition).
 * @i: Index a point should be inserted.
 * @pos: Position the point should be inserted to.
 *
 * Fixes position of a color point between neighbours and to range 0..1.
 *
 * Returns: Fixed position.
 **/
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
 **/
void
gwy_gradient_set_point(GwyGradient *gradient,
                       gint index_,
                       const GwyGradientPoint *point)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
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
 **/
void
gwy_gradient_set_point_color(GwyGradient *gradient,
                             gint index_,
                             const GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
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
 **/
void
gwy_gradient_insert_point(GwyGradient *gradient,
                          gint index_,
                          GwyGradientPoint *point)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
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
 **/
gint
gwy_gradient_insert_point_sorted(GwyGradient *gradient,
                                 GwyGradientPoint *point)
{
    GArray *points;
    GwyGradientPoint pt;
    gint i;

    g_return_val_if_fail(GWY_IS_GRADIENT(gradient), -1);
    g_return_val_if_fail(!GWY_RESOURCE(gradient)->is_const, -1);
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
 **/
void
gwy_gradient_delete_point(GwyGradient *gradient,
                          gint index_)
{
    GArray *points;
    GwyGradientPoint *pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
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
 **/
void
gwy_gradient_reset(GwyGradient *gradient)
{
    GwyGradientPoint pt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);

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
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);

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
 **/
void
gwy_gradient_set_points(GwyGradient *gradient,
                        gint npoints,
                        const GwyGradientPoint *points)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
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
 **/
void
gwy_gradient_set_from_samples(GwyGradient *gradient,
                              gint nsamples,
                              guchar *samples)
{
    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
    g_return_if_fail(samples);
    g_return_if_fail(nsamples > 0);

    /* TODO */
    g_warning("Implement me!");

    gwy_gradient_changed(gradient);
}

static void
gwy_gradient_changed(GwyGradient *gradient)
{
    gwy_debug("%s", gradient->name);
    if (gradient->pixels)
        gwy_gradient_sample(gradient, GWY_GRADIENT_DEFAULT_SIZE,
                            gradient->pixels);
    gwy_resource_data_changed(GWY_RESOURCE(gradient));
}

static void
gwy_gradient_preset(const gchar *name,
                    gint npoints,
                    const GwyGradientPoint *points)
{
    static GwyInventory *inventory = NULL;
    GwyGradient *gradient;

    gradient = gwy_gradient_new(name, npoints, points);
    if (!inventory)
        inventory = GWY_RESOURCE_GET_CLASS(gradient)->inventory;
    GWY_RESOURCE(gradient)->is_const = TRUE;
    GWY_RESOURCE(gradient)->is_modified = FALSE;
    /* FIXME */
    gwy_resource_set_is_preferred(GWY_RESOURCE(gradient), TRUE);
    gwy_inventory_insert_item(inventory, gradient);
    g_object_unref(gradient);
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
 **/
void
_gwy_gradients_setup_presets(void)
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
    gpointer klass;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_GRADIENT);
    gwy_inventory_forget_order(gwy_gradients());

    gwy_gradient_preset(GWY_GRADIENT_DEFAULT, G_N_ELEMENTS(gray), gray);
    gwy_gradient_preset("Rainbow1", G_N_ELEMENTS(rainbow1), rainbow1),
    gwy_gradient_preset("Rainbow2", G_N_ELEMENTS(rainbow2), rainbow2);
    gwy_gradient_preset("Gold", G_N_ELEMENTS(gold), gold);
    gwy_gradient_preset("Pm3d", G_N_ELEMENTS(pm3d), pm3d);
    gwy_gradient_preset("Spectral", G_N_ELEMENTS(spectral), spectral);
    gwy_gradient_preset("Warm", G_N_ELEMENTS(warm), warm);
    gwy_gradient_preset("Cold", G_N_ELEMENTS(cold), cold);
    gwy_gradient_preset("DFit", G_N_ELEMENTS(dfit), dfit);

    gwy_gradient_preset("Spring", G_N_ELEMENTS(spring), spring);
    gwy_gradient_preset("Body", G_N_ELEMENTS(body), body);
    gwy_gradient_preset("Sky", G_N_ELEMENTS(sky), sky);
    gwy_gradient_preset("Lines", G_N_ELEMENTS(lines), lines);

    pd[1].color = red_color;
    gwy_gradient_preset("Red", G_N_ELEMENTS(pd), pd);
    pd[1].color = green_color;
    gwy_gradient_preset("Green", G_N_ELEMENTS(pd), pd);
    pd[1].color = blue_color;
    gwy_gradient_preset("Blue", G_N_ELEMENTS(pd), pd);
    pd[1].color = xyellow;
    gwy_gradient_preset("Yellow", G_N_ELEMENTS(pd), pd);
    pd[1].color = pink;
    gwy_gradient_preset("Pink", G_N_ELEMENTS(pd), pd);
    pd[1].color = olive;
    gwy_gradient_preset("Olive", G_N_ELEMENTS(pd), pd);

    pd3[1].color = red_color;
    pd3[2].color = yellow_color;
    gwy_gradient_preset("Red-Yellow", G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = violet_color;
    gwy_gradient_preset("Red-Violet", G_N_ELEMENTS(pd3), pd3);

    pd3[1].color = blue_color;
    pd3[2].color = cyan_color;
    gwy_gradient_preset("Blue-Cyan", G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = violet_color;
    gwy_gradient_preset("Blue-Violet", G_N_ELEMENTS(pd3), pd3);

    pd3[1].color = green_color;
    pd3[2].color = yellow_color;
    gwy_gradient_preset("Green-Yellow", G_N_ELEMENTS(pd3), pd3);
    pd3[2].color = cyan_color;
    gwy_gradient_preset("Green-Cyan", G_N_ELEMENTS(pd3), pd3);

    pd4[1].color = red_color;
    pd4[3].color = cyan_color;
    gwy_gradient_preset("Red-Cyan", G_N_ELEMENTS(pd4), pd4);
    pd4[1].color = blue_color;
    pd4[3].color = yellow_color;
    gwy_gradient_preset("Blue-Yellow", G_N_ELEMENTS(pd4), pd4);
    pd4[1].color = green_color;
    pd4[3].color = violet_color;
    gwy_gradient_preset("Green-Violet", G_N_ELEMENTS(pd4), pd4);

    pd2 = g_new(GwyGradientPoint, 20);
    for (i = 0; i < 10; i++) {
        pd2[i].x = i/9.0;
        pd2[i].color = i%2 ? black_color : white_color;
    }
    gwy_gradient_preset("BW1", 10, pd2);

    pd2[0].x = 0.0;
    pd2[0].color = black_color;
    for (i = 1; i < 19; i++) {
        pd2[i].x = (i/2 + i%2)/10.0 + (i%2 ? -0.01 : 0.01);
        pd2[i].color = i/2%2 ? white_color : black_color;
    }
    pd2[19].x = 1.0;
    pd2[19].color = white_color;
    gwy_gradient_preset("BW2", 20, pd2);
    g_free(pd2);

    gwy_inventory_restore_order(gwy_gradients());
    g_type_class_unref(klass);
}

/* Eats @name */
static GwyGradient*
gwy_gradient_new(const gchar *name,
                 gint npoints,
                 const GwyGradientPoint *points)
{
    GwyGradient *gradient;

    g_return_val_if_fail(name, NULL);

    gradient = g_object_new(GWY_TYPE_GRADIENT, NULL);
    if (npoints && points) {
        g_array_set_size(gradient->points, 0);
        g_array_append_vals(gradient->points, points, npoints);
    }
    GWY_RESOURCE(gradient)->name = g_strdup(name);
    /* A new resource is modified by default, fixed resources set it back to
     * FALSE */
    GWY_RESOURCE(gradient)->is_modified = TRUE;

    return gradient;
}

gpointer
gwy_gradient_copy(gpointer item)
{
    GwyGradient *gradient, *copy;

    g_return_val_if_fail(GWY_IS_GRADIENT(item), NULL);

    gradient = GWY_GRADIENT(item);
    copy = gwy_gradient_new(GWY_RESOURCE(item)->name,
                            gradient->points->len,
                            (GwyGradientPoint*)gradient->points->data);

    return copy;
}

static void
gwy_gradient_dump(GwyResource *resource,
                  GString *str)
{
    GwyGradient *gradient;
    GwyGradientPoint *pt;
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    guint i;

    g_return_if_fail(GWY_IS_GRADIENT(resource));
    gradient = GWY_GRADIENT(resource);
    g_return_if_fail(gradient->points->len > 0);

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
}

static GwyResource*
gwy_gradient_parse(const gchar *text)
{
    GwyGradient *gradient = NULL;
    GwyGradientClass *klass;
    GArray *points = NULL;
    GwyGradientPoint pt;
    gchar *name = NULL;
    gchar *str, *p, *line, *end;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_GRADIENT);
    g_return_val_if_fail(klass, NULL);

    p = str = g_strdup(text);
    points = g_array_new(FALSE, FALSE, sizeof(GwyGradientPoint));
    while (TRUE) {
        if (!(line = gwy_str_next_line(&p)))
            break;
        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            continue;
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

    gradient = gwy_gradient_new(name,
                                points->len, (GwyGradientPoint*)points->data);

fail:
    if (points)
        g_array_free(points, TRUE);
    g_free(name);
    g_free(str);
    return (GwyResource*)gradient;
}

/**
 * gwy_gradients:
 *
 * Gets inventory with all the gradients.
 *
 * Returns: Gradient inventory.
 **/
GwyInventory*
gwy_gradients(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GRADIENT))->inventory;
}

/************************** Documentation ****************************/

/**
 * GwyGradient:
 *
 * The #GwyGradient struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGradientPoint:
 * @x: Color point position (in interval [0,1]).
 * @color: The color at position @x.
 *
 * Gradient color point struct.
 **/

/**
 * gwy_gradients_get_gradient:
 * @name: Gradient name.  May be %NULL to get default gradient.
 *
 * Convenience macro to get a gradient from gwy_gradients() by name.
 *
 * Returns: Gradient identified by @name or default gradient if it does not
 *          exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
