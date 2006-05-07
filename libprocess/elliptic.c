/*
 *  @(#) $Id$
 *  Copyright (C) 2005-2006 David Necas (Yeti), Petr Klapetek, Chris Anderson.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, sidewinder.asu@gmail.com.
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

/* TODO: we can calculate inner cycle boundaries explicitly and in addition
 * to saving calculations we can use plain memcpy() to copy data. */

#define GWY_DATA_FIELD_RAW_ACCESS
#include "config.h"

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/elliptic.h>

/**
 * gwy_data_field_elliptic_area_fill:
 * @data_field: A data field.
 * @col: Upper-left bounding box column coordinate.
 * @row: Upper-left bounding box row coordinate.
 * @width: Bounding box width (number of columns).
 * @height: Bounding box height (number of rows).
 * @value: Value to be entered.
 *
 * Fills an elliptic region of a data field with given value.
 *
 * The elliptic region is defined by its bounding box which must be completely
 * contained in the data field.
 *
 * Returns: The number of filled values.
 **/
gint
gwy_data_field_elliptic_area_fill(GwyDataField *data_field,
                                  gint col, gint row,
                                  gint width, gint height,
                                  gdouble value)
{
    gint i, j, xres, count;
    gdouble x, y, a, b, a2, b2, s;
    gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         0);

    a = width/2.0;
    a2 = a*a;
    b = height/2.0;
    b2 = b*b;
    xres = data_field->xres;
    count = 0;

    for (i = 0; i < height; i++) {
        d = data_field->data + (row + i)*xres + col;
        y = i - b;
        s = a2*(1.0 - y*y/b2);
        for (j = 0; j < width; j++) {
            x = j - a;
            if (x*x <= s) {
                d[j] = value;
                count++;
            }
        }
    }

    gwy_data_field_invalidate(data_field);

    return count;
}

/**
 * gwy_data_field_elliptic_area_extract:
 * @data_field: A data field.
 * @col: Upper-left bounding box column coordinate.
 * @row: Upper-left bounding box row coordinate.
 * @width: Bounding box width (number of columns).
 * @height: Bounding box height (number of rows).
 * @data: Location to store the extracted values to.  Its size has to be
 *        sufficient to contain all the extracted values.  As a conservative
 *        estimate @width*@height can be used, or the
 *        size can be calculated with gwy_data_field_get_elliptic_area_size().
 *
 * Extracts values from an elliptic region of a data field.
 *
 * The elliptic region is defined by its bounding box which must be completely
 * contained in the data field.
 *
 * Returns: The number of extracted values.
 **/
gint
gwy_data_field_elliptic_area_extract(GwyDataField *data_field,
                                     gint col, gint row,
                                     gint width, gint height,
                                     gdouble *data)
{
    gint i, j, xres, count;
    gdouble x, y, a, b, a2, b2, s;
    const gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         0);

    a = width/2.0;
    a2 = a*a;
    b = height/2.0;
    b2 = b*b;
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    for (i = 0; i < height; i++) {
        d = data_field->data + (row + i)*xres + col;
        y = i - b;
        s = a2*(1.0 - y*y/b2);
        for (j = 0; j < width; j++) {
            x = j - a;
            if (x*x <= s) {
                data[count] = d[j];
                count++;
            }
        }
    }

    return count;
}

/**
 * gwy_data_field_elliptic_area_unextract:
 * @data_field: A data field.
 * @col: Upper-left bounding box column coordinate.
 * @row: Upper-left bounding box row coordinate.
 * @width: Bounding box width (number of columns).
 * @height: Bounding box height (number of rows).
 * @data: The values to put back.  It must be the same array as in previous
 *        gwy_data_field_elliptic_area_extract().
 *
 * Puts values back to an elliptic region of a data field.
 *
 * The elliptic region is defined by its bounding box which must be completely
 * contained in the data field.
 *
 * This method does the reverse of gwy_data_field_elliptic_area_extract()
 * allowing to implement pixel-wise filters on elliptic areas.  Values from
 * @data are put back to the same positions
 * gwy_data_field_elliptic_area_extract() took them from.
 **/
void
gwy_data_field_elliptic_area_unextract(GwyDataField *data_field,
                                       gint col, gint row,
                                       gint width, gint height,
                                       const gdouble *data)
{
    gint i, j, xres, count;
    gdouble x, y, a, b, a2, b2, s;
    gdouble *d;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    a = width/2.0;
    a2 = a*a;
    b = height/2.0;
    b2 = b*b;
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    for (i = 0; i < height; i++) {
        d = data_field->data + (row + i)*xres + col;
        y = i - b;
        s = a2*(1.0 - y*y/b2);
        for (j = 0; j < width; j++) {
            x = j - a;
            if (x*x <= s) {
                d[j] = data[count];
                count++;
            }
        }
    }
}

/**
 * gwy_data_field_get_elliptic_area_size:
 * @width: Bounding box width.
 * @height: Bounding box height.
 *
 * Calculates an upper bound of the number of samples in an elliptic region.
 *
 * Returns: The number of pixels in an elliptic region with given rectangular
 *          bounds (or its upper bound).
 **/
gint
gwy_data_field_get_elliptic_area_size(gint width,
                                      gint height)
{
    gint i, j, count;
    gdouble x, y, a, b, a2, b2, s;

    if (width <= 0 || height <= 0)
        return 0;

    a = width/2.0;
    a2 = a*a;
    b = height/2.0;
    b2 = b*b;
    count = 0;

    for (i = 0; i < height; i++) {
        y = i - b;
        s = a2*(1.0 - y*y/b2);
        for (j = 0; j < width; j++) {
            x = j - a;
            if (x*x <= s)
                count++;
        }
    }

    return count;
}

/**
 * gwy_data_field_circular_area_fill:
 * @data_field: A data field.
 * @col: Row index of circular area center.
 * @row: Column index of circular area center.
 * @radius: Circular area radius (in pixels).  Any value is allowed, although
 *          to get areas that do not deviate from true circles after
 *          pixelization too much, half-integer values are recommended,
 *          integer values are NOT recommended.
 * @value: Value to be entered.
 *
 * Fills an elliptic region of a data field with given value.
 *
 * Returns: The number of filled values.
 **/
gint
gwy_data_field_circular_area_fill(GwyDataField *data_field,
                                  gint col, gint row,
                                  gdouble radius,
                                  gdouble value)
{
    gint i, j, r, r2, count, xres;
    gint ifrom, jfrom, ito, jto;
    gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);

    if (radius < 0.0)
        return 0;

    r2 = floor(radius*radius + 1e-12);
    r = floor(radius + 1e-12);
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    /* Clip */
    ifrom = MAX(row - r, 0) - row;
    jfrom = MAX(col - r, 0) - col;
    ito = MIN(row + r, data_field->yres-1) - row;
    jto = MIN(col + r, data_field->xres-1) - col;

    for (i = ifrom; i <= ito; i++) {
        for (j = jfrom; j <= jto; j++) {
            if (i*i + j*j <= r2) {
                d[(row + i)*xres + col + j] = value;
                count++;
            }
        }
    }

    return count;
}

/**
 * gwy_data_field_circular_area_extract:
 * @data_field: A data field.
 * @col: Row index of circular area center.
 * @row: Column index of circular area center.
 * @radius: Circular area radius (in pixels).  Any value is allowed, although
 *          to get areas that do not deviate from true circles after
 *          pixelization too much, half-integer values are recommended,
 *          integer radii are NOT recommended.
 * @data: Location to store the extracted values to.  Its size has to be
 *        sufficient to contain all the extracted values.  As a conservative
 *        estimate (2*floor(@radius)+1)^2 can be used, or the size can be
 *        calculated with gwy_data_field_get_circular_area_size().
 *
 * Extracts values from a circular region of a data field.
 *
 * Returns: The number of extracted values.  It can be zero when the inside of
 *          the circle does not intersect with the data field.
 **/
gint
gwy_data_field_circular_area_extract(GwyDataField *data_field,
                                     gint col, gint row,
                                     gdouble radius,
                                     gdouble *data)
{
    gint i, j, r, r2, count, xres;
    gint ifrom, jfrom, ito, jto;
    const gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(data, 0);

    if (radius < 0.0)
        return 0;

    r2 = floor(radius*radius + 1e-12);
    r = floor(radius + 1e-12);
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    /* Clip */
    ifrom = MAX(row - r, 0) - row;
    jfrom = MAX(col - r, 0) - col;
    ito = MIN(row + r, data_field->yres-1) - row;
    jto = MIN(col + r, data_field->xres-1) - col;

    for (i = ifrom; i <= ito; i++) {
        for (j = jfrom; j <= jto; j++) {
            if (i*i + j*j <= r2) {
                data[count] = d[(row + i)*xres + col + j];
                count++;
            }
        }
    }

    return count;
}

/**
 * gwy_data_field_circular_area_unextract:
 * @data_field: A data field.
 * @col: Row index of circular area center.
 * @row: Column index of circular area center.
 * @radius: Circular area radius (in pixels).
 * @data: The values to put back.  It must be the same array as in previous
 *        gwy_data_field_circular_area_unextract().
 *
 * Puts values back to a circular region of a data field.
 *
 * This method does the reverse of gwy_data_field_circular_area_extract()
 * allowing to implement pixel-wise filters on circular areas.  Values from
 * @data are put back to the same positions
 * gwy_data_field_circular_area_extract() took them from.
 **/
void
gwy_data_field_circular_area_unextract(GwyDataField *data_field,
                                       gint col, gint row,
                                       gdouble radius,
                                       const gdouble *data)
{
    gint i, j, r, r2, count, xres;
    gint ifrom, jfrom, ito, jto;
    gdouble *d;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(data);

    if (radius < 0.0)
        return;

    r2 = floor(radius*radius + 1e-12);
    r = floor(radius + 1e-12);
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    /* Clip */
    ifrom = MAX(row - r, 0) - row;
    jfrom = MAX(col - r, 0) - col;
    ito = MIN(row + r, data_field->yres-1) - row;
    jto = MIN(col + r, data_field->xres-1) - col;

    for (i = ifrom; i <= ito; i++) {
        for (j = jfrom; j <= jto; j++) {
            if (i*i + j*j <= r2) {
                d[(row + i)*xres + col + j] = data[count];
                count++;
            }
        }
    }
}

/**
 * gwy_data_field_get_circular_area_size:
 * @radius: Circular area radius (in pixels).
 *
 * Calculates an upper bound of the number of samples in a circular region.
 *
 * Returns: The number of pixels in a circular region with given rectangular
 *          bounds (or its upper bound).
 **/
gint
gwy_data_field_get_circular_area_size(gdouble radius)
{
    gint i, j, r, r2, count;

    if (radius < 0.0)
        return 0;

    r2 = floor(radius*radius + 1e-12);
    r = floor(radius + 1e-12);
    count = 0;

    for (i = -r; i <= r; i++) {
        for (j = -r; j <= r; j++) {
            if (i*i + j*j <= r2)
                count++;
        }
    }

    return count;
}

/************************** Documentation ****************************/

/**
 * SECTION:elliptic
 * @title: elliptic
 * @short_description: Functions to work with elliptic areas
 *
 * Method for extraction and putting back data from/to elliptic and circular
 * areas can be used to implement sample-wise operations, that is operations
 * that depend only on sample value not on its position, on these areas:
 * <informalexample><programlisting>
 * gdouble *data;
 * gint n, i;
 * <!-- Hello, gtk-doc! -->
 * data = g_new(gdouble, width*height);
 * n = gwy_data_field_elliptic_area_extract(data_field,
 *                                          col, row, width, height,
 *                                          data);
 * for (i = 0; i < n; i++) {
 *    ... do something with data[i] ...
 * }
 * gwy_data_field_elliptic_area_unextract(data_field,
 *                                        col, row, width, height,
 *                                        data);
 * </programlisting></informalexample>
 *
 * Another possibility is to use #GwyDataLine methods on the extracted data
 * (in practice one would use the same data line repeatedly, of course):
 * <informalexample><programlisting>
 * GwyDataLine *data_line;
 * gdouble *data;
 * gint n;
 * <!-- Hello, gtk-doc! -->
 * n = gwy_data_field_get_elliptic_area_size(data_field, width, height);
 * data_line = gwy_data_line_new(n, 1.0, FALSE);
 * data = gwy_data_line_get_data(data_line);
 * gwy_data_field_elliptic_area_extract(data_field,
 *                                      col, row, width, height,
 *                                      data);
 * gwy_data_line_pixelwise_filter(data_line, ...);
 * gwy_data_field_elliptic_area_unextract(data_field,
 *                                        col, row, width, height,
 *                                        data);
 * g_object_unref(data_line);
 * </programlisting></informalexample>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
