/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwyrgba.h"

static gboolean quarks_initialized = FALSE;
static GQuark sett_keys[4];
static GQuark data_keys[4];

static void initialize_quarks(void);

GType
gwy_rgba_get_type (void)
{
  static GType rgba_type = 0;

  if (rgba_type == 0)
    rgba_type = g_boxed_type_register_static("GwyRGBA",
                                            (GBoxedCopyFunc)gwy_rgba_copy,
                                            (GBoxedFreeFunc)gwy_rgba_free);
  return rgba_type;
}

/**
 * gwy_rgba_copy:
 * @rgba: A #GwyRGBA.
 *
 * Makes a copy of a rgba structure. The result must be freed using
 * gwy_rgba_free().
 *
 * XXX: Just kidding, we curently dont' use memchunks.
 *
 * Returns: A copy of @rgba.
 **/
GwyRGBA*
gwy_rgba_copy(const GwyRGBA *rgba)
{
    g_return_val_if_fail(rgba, NULL);
    return g_memdup(rgba, sizeof(GwyRGBA));
}

/**
 * gwy_rgba_free:
 * @rgba: A #GwyRGBA.
 *
 * Frees an rgba structure created with gwy_rgba_copy().
 **/
void
gwy_rgba_free(GwyRGBA *rgba)
{
    g_return_if_fail(rgba);
    g_free(rgba);
}

/**
 * gwy_rgba_to_gdk_color:
 * @rgba: A #GwyRGBA.
 * @gdkcolor: A #GdkColor.
 *
 * Converts a rgba to a Gdk color.
 *
 * Note no allocation is performed, just channel value conversion.
 **/
void
gwy_rgba_to_gdk_color(const GwyRGBA *rgba,
                      GdkColor *gdkcolor)
{
    gdkcolor->red   = (guint16)floor(rgba->r*65535.999999);
    gdkcolor->green = (guint16)floor(rgba->g*65535.999999);
    gdkcolor->blue  = (guint16)floor(rgba->b*65535.999999);
    gdkcolor->pixel = (guint32)-1;
}

/**
 * gwy_rgba_to_gdk_alpha:
 * @rgba: A #GwyRGBA.
 *
 * Converts a rgba to a Gdk opacity value.
 *
 * Returns: The opacity value as a 16bit integer.
 **/
guint16
gwy_rgba_to_gdk_alpha(const GwyRGBA *rgba)
{
    return (guint16)floor(rgba->a*65535.999999);
}

/**
 * gwy_rgba_from_gdk_color:
 * @rgba: A #GwyRGBA.
 * @gdkcolor: A #GdkColor.
 *
 * Converts a Gdk color to a rgba.
 *
 * The alpha value is unchanged, as #GdkColor has no opacity information.
 **/
void
gwy_rgba_from_gdk_color(GwyRGBA *rgba,
                        const GdkColor *gdkcolor)
{
    rgba->r = gdkcolor->red/65535.0;
    rgba->g = gdkcolor->green/65535.0;
    rgba->b = gdkcolor->blue/65535.0;
}

/**
 * gwy_rgba_from_gdk_color_and_alpha:
 * @rgba: A #GwyRGBA.
 * @gdkcolor: A #GdkColor.
 * @gdkalpha: Gdk 16bit opacity value.
 *
 * Converts a Gdk color plus an opacity value to a rgba.
 **/
void
gwy_rgba_from_gdk_color_and_alpha(GwyRGBA *rgba,
                                  const GdkColor *gdkcolor,
                                  guint16 gdkalpha)
{
    rgba->r = gdkcolor->red/65535.0;
    rgba->g = gdkcolor->green/65535.0;
    rgba->b = gdkcolor->blue/65535.0;
    rgba->a = gdkalpha/65535.0;
}

/**
 * gwy_rgba_interpolate:
 * @src1: Color at point @x = 0.0.
 * @src2: Color at point @x = 1.0.
 * @x: Point in interval 0..1 to take color from.
 * @rgba: A #GwyRGBA to store result to.
 *
 * Linearly interpolates two colors, including alpha blending.
 *
 * Correct blending of two not fully opaque colors is tricky.  Always use
 * this function, not simple independent interpolation of r, g, b, and a.
 **/
void
gwy_rgba_interpolate(const GwyRGBA *src1,
                     const GwyRGBA *src2,
                     gdouble x,
                     GwyRGBA *rgba)
{
    /* for alpha = 0.0 there's actually no limit, but average is psychologicaly
     * better than some random value */
    if (G_LIKELY(src1->a == src2->a)) {
        rgba->a = src1->a;
        rgba->r = x*src2->r + (1.0 - x)*src1->r;
        rgba->g = x*src2->g + (1.0 - x)*src1->g;
        rgba->b = x*src2->b + (1.0 - x)*src1->b;
        return;
    }

    if (src2->a == 0.0) {
        rgba->a = (1.0 - x)*src1->a;
        rgba->r = src1->r;
        rgba->g = src1->g;
        rgba->b = src1->b;
        return;
    }
    if (src1->a == 0.0) {
        rgba->a = x*src2->a;
        rgba->r = src2->r;
        rgba->g = src2->g;
        rgba->b = src2->b;
        return;
    }

    /* nothing helped, it's a general case
     * however, for meaningful values, rgba->a cannot be 0.0 */
    rgba->a = x*src2->a + (1.0 - x)*src1->a;
    rgba->r = (x*src2->a*src2->r + (1.0 - x)*src1->a*src1->r)/rgba->a;
    rgba->g = (x*src2->a*src2->g + (1.0 - x)*src1->a*src1->g)/rgba->a;
    rgba->b = (x*src2->a*src2->b + (1.0 - x)*src1->a*src1->b)/rgba->a;
}

/**
 * gwy_rgba_get_from_container:
 * @rgba: A #GwyRGBA.
 * @container: A #GwyContainer to get the color components from.
 * @prefix: Prefix in @container, e.g. "/0/mask" (it would try to fetch
 *          "/0/mask/red", "/0/mask/green", etc. then).
 *
 * Gets RGBA color components from a container.
 *
 * Returns: Whether all @rgba components were successfully found and set.
 **/
gboolean
gwy_rgba_get_from_container(GwyRGBA *rgba,
                            GwyContainer *container,
                            const gchar *prefix)
{
    gchar *buffer;
    gsize len;
    gboolean ok;

    gwy_debug("");
    g_return_val_if_fail(rgba && container && prefix, FALSE);

    if (!quarks_initialized)
        initialize_quarks();

    /* optimize for common cases */
    if (!strcmp(prefix, "/0/mask")) {
        return gwy_container_gis_double(container, data_keys[0], &rgba->r)
               && gwy_container_gis_double(container, data_keys[1], &rgba->g)
               && gwy_container_gis_double(container, data_keys[2], &rgba->b)
               && gwy_container_gis_double(container, data_keys[3], &rgba->a);
    }
    if (!strcmp(prefix, "/mask")) {
        return gwy_container_gis_double(container, sett_keys[0], &rgba->r)
               && gwy_container_gis_double(container, sett_keys[1], &rgba->g)
               && gwy_container_gis_double(container, sett_keys[2], &rgba->b)
               && gwy_container_gis_double(container, sett_keys[3], &rgba->a);
    }

    /* quarkize keys */
    ok = TRUE;
    len = strlen(prefix);
    buffer = g_new(gchar, len + 6 + 1);
    strcpy(buffer, prefix);
    strcpy(buffer + len, "/red");
    ok &= gwy_container_gis_double_by_name(container, buffer, &rgba->r);
    strcpy(buffer + len, "/green");
    ok &= gwy_container_gis_double_by_name(container, buffer, &rgba->g);
    strcpy(buffer + len, "/blue");
    ok &= gwy_container_gis_double_by_name(container, buffer, &rgba->b);
    strcpy(buffer + len, "/alpha");
    ok &= gwy_container_gis_double_by_name(container, buffer, &rgba->a);
    g_free(buffer);

    return ok;
}

/**
 * gwy_rgba_store_to_container:
 * @rgba: A #GwyRGBA.
 * @container: A #GwyContainer to store the color components to.
 * @prefix: Prefix in @container, e.g. "/0/mask" (it would try to store
 *          "/0/mask/red", "/0/mask/green", etc. then).
 *
 * Stores RGBA color components to a container.
 **/
void
gwy_rgba_store_to_container(const GwyRGBA *rgba,
                            GwyContainer *container,
                            const gchar *prefix)
{
    gchar *buffer;
    gsize len;

    gwy_debug("");
    g_return_if_fail(rgba && container && prefix);

    if (!quarks_initialized)
        initialize_quarks();

    /* optimize for common cases */
    if (!strcmp(prefix, "/0/mask")) {
        gwy_container_set_double(container, data_keys[0], rgba->r);
        gwy_container_set_double(container, data_keys[1], rgba->g);
        gwy_container_set_double(container, data_keys[2], rgba->b);
        gwy_container_set_double(container, data_keys[3], rgba->a);
        return;
    }
    if (!strcmp(prefix, "/mask")) {
        gwy_container_set_double(container, sett_keys[0], rgba->r);
        gwy_container_set_double(container, sett_keys[1], rgba->g);
        gwy_container_set_double(container, sett_keys[2], rgba->b);
        gwy_container_set_double(container, sett_keys[3], rgba->a);
        return;
    }

    /* quarkize keys */
    len = strlen(prefix);
    buffer = g_new(gchar, len + 6 + 1);
    strcpy(buffer, prefix);
    strcpy(buffer + len, "/red");
    gwy_container_set_double_by_name(container, buffer, rgba->r);
    strcpy(buffer + len, "/green");
    gwy_container_set_double_by_name(container, buffer, rgba->g);
    strcpy(buffer + len, "/blue");
    gwy_container_set_double_by_name(container, buffer, rgba->b);
    strcpy(buffer + len, "/alpha");
    gwy_container_set_double_by_name(container, buffer, rgba->a);
    g_free(buffer);
}

static void
initialize_quarks(void)
{
    sett_keys[0] = g_quark_from_static_string("/mask/red");
    sett_keys[1] = g_quark_from_static_string("/mask/green");
    sett_keys[2] = g_quark_from_static_string("/mask/blue");
    sett_keys[3] = g_quark_from_static_string("/mask/alpha");

    data_keys[0] = g_quark_from_static_string("/0/mask/red");
    data_keys[1] = g_quark_from_static_string("/0/mask/green");
    data_keys[2] = g_quark_from_static_string("/0/mask/blue");
    data_keys[3] = g_quark_from_static_string("/0/mask/alpha");

    quarks_initialized = TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyRGBA:
 * @r: The red component.
 * @g: The green component.
 * @b: The blue component.
 * @a: The alpha (opacity) value.
 *
 * RGB[A] color specification type.
 *
 * All values are from the range [0,1].
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
