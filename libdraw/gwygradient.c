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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libdraw/gwygradient.h>
#include "gwydrawinternal.h"

/* Standard sample size, returned by gwy_gradient_get_samples() */
enum {
    GWY_GRADIENT_DEFAULT_SIZE = 1024
};

#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))

static void         gwy_gradient_finalize       (GObject *object);
static gpointer     gwy_gradient_copy           (gpointer);
static void         gwy_gradient_use            (GwyResource *resource);
static void         gwy_gradient_release        (GwyResource *resource);
static void         gwy_gradient_sample_real    (GwyGradient *gradient,
                                                 gint nsamples,
                                                 guchar *samples);
static void         gwy_gradient_sanitize       (GwyGradient *gradient);
static void         gwy_gradient_refine_interval(GList *points,
                                                 gint n,
                                                 const GwyGradientPoint *samples,
                                                 gdouble threshold);
static void         gwy_gradient_changed        (GwyGradient *gradient);
static GwyGradient* gwy_gradient_new            (const gchar *name,
                                                 gint npoints,
                                                 const GwyGradientPoint *points,
                                                 gboolean is_const);
static void         gwy_gradient_dump           (GwyResource *resource,
                                                 GString *str);
static GwyResource* gwy_gradient_parse          (const gchar *text,
                                                 gboolean is_const);


static const GwyRGBA black_color = { 0, 0, 0, 1 };
static const GwyRGBA white_color = { 1, 1, 1, 1 };
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

    res_class->name = "gradients";
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
    g_array_append_val(gradient->points, pt);
    pt.x = 1.0;
    pt.color = white_color;
    g_array_append_val(gradient->points, pt);
}

static void
gwy_gradient_finalize(GObject *object)
{
    GwyGradient *gradient = (GwyGradient*)object;

    g_array_free(gradient->points, TRUE);
    G_OBJECT_CLASS(gwy_gradient_parent_class)->finalize(object);
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
        x = MIN(i*q, 1.0);
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
    for (i = 1; i < height; i++)
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
 * It is an error to try to move points beyond its neighbours, or to move first
 * (or last) point from 0 (or 1).
 **/
void
gwy_gradient_set_point(GwyGradient *gradient,
                       gint index_,
                       const GwyGradientPoint *point)
{
    GwyGradientPoint pt, *gradpt;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
    g_return_if_fail(point);
    g_return_if_fail(index_ >= 0 && index_ < gradient->points->len);

    pt.color = gwy_gradient_fix_rgba(&point->color);
    pt.x = gwy_gradient_fix_position(gradient->points, index_, point->x);
    gradpt = &g_array_index(gradient->points, GwyGradientPoint, index_);
    if (gradpt->x == pt.x
        && gradpt->color.r == pt.color.r
        && gradpt->color.g == pt.color.g
        && gradpt->color.b == pt.color.b)
        return;

    *gradpt = pt;
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
    GwyGradientPoint *gradpt;
    GwyRGBA rgba;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
    g_return_if_fail(color);
    g_return_if_fail(index_ >= 0 && index_ < gradient->points->len);

    gradpt = &g_array_index(gradient->points, GwyGradientPoint, index_);
    rgba = gwy_gradient_fix_rgba(color);
    if (gradpt->color.r == rgba.r
        && gradpt->color.g == rgba.g
        && gradpt->color.b == rgba.b)
        return;

    gradpt->color = rgba;
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
                          const GwyGradientPoint *point)
{
    GwyGradientPoint pt, tmp;

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
    tmp = g_array_index(gradient->points, GwyGradientPoint, index_);
    g_array_insert_val(gradient->points, index_, tmp);
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
                                 const GwyGradientPoint *point)
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
 * @nsamples: Number of samples, it must be at least one.
 * @samples: Sampled color gradient in #GdkPixbuf-like RRGGBBAA form.
 * @threshold: Maximum allowed difference (for color components in range 0..1).
 *             When negative, default value 1/80 suitable for most purposes
 *             is used.
 *
 * Reconstructs color gradient definition from sampled colors.
 *
 * The result is usually approximate.
 **/
void
gwy_gradient_set_from_samples(GwyGradient *gradient,
                              gint nsamples,
                              const guchar *samples,
                              gdouble threshold)
{
    GwyGradientPoint *spoints;
    GList *l, *list = NULL;
    gint i, k;

    g_return_if_fail(GWY_IS_GRADIENT(gradient));
    g_return_if_fail(!GWY_RESOURCE(gradient)->is_const);
    g_return_if_fail(samples);
    g_return_if_fail(nsamples > 0);

    if (threshold < 0)
        threshold = 1.0/80.0;

    /* Preprocess guchar data to doubles */
    spoints = g_new(GwyGradientPoint, MAX(nsamples, 2));
    for (k = i = 0; i < nsamples; i++) {
        spoints[i].x = i/(nsamples - 1.0);
        spoints[i].color.r = samples[k++]/255.0;
        spoints[i].color.g = samples[k++]/255.0;
        spoints[i].color.b = samples[k++]/255.0;
        spoints[i].color.a = samples[k++]/255.0;
    }

    /* Handle special silly case */
    if (nsamples == 1) {
        spoints[0].x = 0.0;
        spoints[1].x = 1.0;
        spoints[1].color = spoints[0].color;
        nsamples = 2;
    }

    /* Start with first and last point and recurse */
    list = g_list_append(list, spoints + nsamples-1);
    list = g_list_prepend(list, spoints);
    gwy_gradient_refine_interval(list, nsamples, spoints, threshold);

    /* Set the new points */
    g_array_set_size(gradient->points, 0);
    for (l = list; l; l = g_list_next(l)) {
        GwyGradientPoint pt;

        pt = *(GwyGradientPoint*)l->data;
        g_array_append_val(gradient->points, pt);
    }
    g_list_free(list);
    g_free(spoints);
    gwy_gradient_changed(gradient);
}

static void
gwy_gradient_refine_interval(GList *points,
                             gint n,
                             const GwyGradientPoint *samples,
                             gdouble threshold)
{
    GList *item;
    const GwyRGBA *first, *last;
    GwyRGBA color;
    gint i, mi;
    gdouble max, s, d;

    if (n <= 2)
        return;

    first = &samples[0].color;
    last = &samples[n-1].color;
    gwy_debug("Working on %d samples from {%f: %f, %f, %f, %f} "
              "to {%f: %f, %f, %f, %f}",
              n,
              samples[0].x, first->r, first->g, first->b, first->a,
              samples[n-1].x, last->r, last->g, last->b, last->a);

    max = 0.0;
    mi = 0;
    for (i = 1; i < n-1; i++) {
        gwy_rgba_interpolate(first, last, i/(n - 1.0), &color);
        /* Maximum distance is the crucial metric */
        s = ABS(color.r - samples[i].color.r);
        d = ABS(color.g - samples[i].color.g);
        s = MAX(s, d);
        d = ABS(color.b - samples[i].color.b);
        s = MAX(s, d);
        d = ABS(color.a - samples[i].color.a);
        s = MAX(s, d);

        if (s > max) {
            max = s;
            mi = i;
        }
    }
    gwy_debug("Max. difference %f located at %d, {%f: %f, %f, %f, %f}",
              max, mi,
              samples[mi].x,
              samples[mi].color.r,
              samples[mi].color.g,
              samples[mi].color.b,
              samples[mi].color.a);

    if (max < threshold) {
        gwy_debug("Max. difference small enough, stopping recursion");
        return;
    }
    gwy_debug("Inserting new point at %f", samples[mi].x);

    /* Use g_list_alloc() manually, GList functions care too much about list
     * head which is something we don't want here, because we always work
     * in the middle of some list. */
    item = g_list_alloc();
    item->data = (gpointer)(samples + mi);
    item->prev = points;
    item->next = points->next;
    item->prev->next = item;
    item->next->prev = item;

    /* Recurse. */
    gwy_gradient_refine_interval(points, mi + 1, samples, threshold);
    gwy_gradient_refine_interval(item, n - mi, samples + mi, threshold);
}

static void
gwy_gradient_changed(GwyGradient *gradient)
{
    gwy_debug("%s", GWY_RESOURCE(gradient)->name->str);
    if (gradient->pixels)
        gwy_gradient_sample(gradient, GWY_GRADIENT_DEFAULT_SIZE,
                            gradient->pixels);
    gwy_resource_data_changed(GWY_RESOURCE(gradient));
}

void
_gwy_gradient_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyGradient *gradient;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_GRADIENT);

    gradient = gwy_gradient_new(GWY_GRADIENT_DEFAULT, 0, NULL, TRUE);
    gwy_inventory_insert_item(klass->inventory, gradient);
    g_object_unref(gradient);

    /* The gradient added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

static GwyGradient*
gwy_gradient_new(const gchar *name,
                 gint npoints,
                 const GwyGradientPoint *points,
                 gboolean is_const)
{
    GwyGradient *gradient;

    g_return_val_if_fail(name, NULL);

    gradient = g_object_new(GWY_TYPE_GRADIENT, "is-const", is_const, NULL);
    if (npoints && points) {
        g_array_set_size(gradient->points, 0);
        g_array_append_vals(gradient->points, points, npoints);
    }
    g_string_assign(GWY_RESOURCE(gradient)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(gradient)->is_modified = !is_const;

    return gradient;
}

gpointer
gwy_gradient_copy(gpointer item)
{
    GwyGradient *gradient, *copy;

    g_return_val_if_fail(GWY_IS_GRADIENT(item), NULL);

    gradient = GWY_GRADIENT(item);
    copy = gwy_gradient_new(gwy_resource_get_name(GWY_RESOURCE(item)),
                            gradient->points->len,
                            (GwyGradientPoint*)gradient->points->data,
                            FALSE);

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
        g_ascii_formatd(buffer, sizeof(buffer), "%.6g", pt->x);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_formatd(buffer, sizeof(buffer), "%.6g", pt->color.r);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_formatd(buffer, sizeof(buffer), "%.6g", pt->color.g);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_formatd(buffer, sizeof(buffer), "%.6g", pt->color.b);
        g_string_append(str, buffer);
        g_string_append_c(str, ' ');
        g_ascii_formatd(buffer, sizeof(buffer), "%g", pt->color.a);
        g_string_append(str, buffer);
        g_string_append_c(str, '\n');
    }
}

static GwyResource*
gwy_gradient_parse(const gchar *text,
                   gboolean is_const)
{
    GwyGradient *gradient = NULL;
    GwyGradientClass *klass;
    GArray *points = NULL;
    GwyGradientPoint pt;
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
        g_warning("Cannot parse color points.");
        goto fail;
    }

    gradient = gwy_gradient_new("",
                                points->len, (GwyGradientPoint*)points->data,
                                is_const);
    gwy_gradient_sanitize(gradient);

fail:
    if (points)
        g_array_free(points, TRUE);
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

/**
 * gwy_gradients_get_gradient:
 * @name: Gradient name.  May be %NULL to get default gradient.
 *
 * Convenience function to get a gradient from gwy_gradients() by name.
 *
 * Returns: Gradient identified by @name or default gradient if it does not
 *          exist.
 **/
GwyGradient*
gwy_gradients_get_gradient(const gchar *name)
{
    GwyInventory *i;

    i = GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GRADIENT))->inventory;
    return (GwyGradient*)gwy_inventory_get_item_or_default(i, name);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygradient
 * @title: GwyGradient
 * @short_description: A map from numbers to RGBA colors
 * @see_also: <link linkend="libgwydraw-gwypixfield">gwypixfield</link> --
 *            Drawing data with gradients,
 *            #GwyInventory -- the container holding all gradients,
 *            #GwyDataView -- 2D data display widget,
 *            #GwyColorAxis -- false color axis widget
 *
 * Gradient is a map from interval [0,1] to RGB(A) color space. Each gradient
 * is defined by an ordered set of color points, the first of them is always at
 * 0.0, the last at 1.0 (thus each gradient must consist of at least two
 * points).  Between them, the color is interpolated.  Color points of
 * modifiable gradients (see below) can be edited with functions like
 * gwy_gradient_insert_point(), gwy_gradient_set_point_color(), or
 * gwy_gradient_set_points().
 *
 * Gradient objects can be obtained from gwy_gradients_get_gradient(). New
 * gradients can be created with gwy_inventory_new_item() on the #GwyInventory
 * returned by gwy_gradients().
 **/

/**
 * GwyGradient:
 *
 * The #GwyGradient struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGradientClass:
 *
 * #GwyGradientClass does not contain any public members.
 **/

/**
 * GwyGradientPoint:
 * @x: Color point position (in interval [0,1]).
 * @color: The color at position @x.
 *
 * Gradient color point struct.
 **/

/**
 * GWY_GRADIENT_DEFAULT:
 *
 * The name of the default gray color gradient.
 *
 * It is guaranteed to always exist.
 *
 * Note this is not the same as user's default gradient which corresponds to
 * the default item in gwy_gradients() inventory and it change over time.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
