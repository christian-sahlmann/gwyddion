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

#define GWY_DATA_FIELD_RAW_ACCESS
#include "config.h"

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/elliptic.h>

/**
 * gwy_data_field_elliptic_area_fill:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
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
                                  gint ulcol, gint ulrow,
                                  gint brcol, gint brrow,
                                  gdouble value)
{
    gint i, j, xres, count;
    gdouble x, y, a, b, a2, b2;
    gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol <= data_field->xres
                         && brrow <= data_field->yres,
                         0);

    a = (gdouble)(brcol - ulcol)/2;
    a2 = a*a;
    b = (gdouble)(brrow - ulrow)/2;
    b2 = a*a;
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    for (i = 0; i < brcol - ulcol; i++) {
        for (j = 0; j < brrow - ulrow; j++) {
            x = (gdouble)i - a;
            y = (gdouble)j - b;

            if (x*x/a2 + y*y/b2 <= 1) {
                d[(j + ulrow)*xres + ulcol + i] = value;
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
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @data: Location to store the extracted values to.  Its size has to be
 *        sufficient to contain all the extracted values.  As a conservative
 *        estimate (@brcol-@ulcol+1)(@brrow-@ulrow+1) is the recommended size.
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
                                     gint ulcol, gint ulrow,
                                     gint brcol, gint brrow,
                                     gdouble *data)
{
    gint i, j, xres, count;
    gdouble x, y, a, b, a2, b2;
    const gdouble *d;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(data, 0);

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol <= data_field->xres
                         && brrow <= data_field->yres,
                         0);

    a = (gdouble)(brcol - ulcol)/2;
    a2 = a*a;
    b = (gdouble)(brrow - ulrow)/2;
    b2 = a*a;
    xres = data_field->xres;
    d = data_field->data;
    count = 0;

    for (i = 0; i < brcol - ulcol; i++) {
        for (j = 0; j < brrow - ulrow; j++) {
            x = (gdouble)i - a;
            y = (gdouble)j - b;

            if (x*x/a2 + y*y/b2 <= 1) {
                data[count] = d[(j + ulrow)*xres + ulcol + i];
                count++;
            }
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
 *          integer values are NOT recommended.
 * @data: Location to store the extracted values to.  Its size has to be
 *        sufficient to contain all the extracted values.  As a conservative
 *        estimate (2*floor(@radius)+1)^2 is the recommended size.
 *
 * Extracts values from an elliptic region of a data field.
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

/************************** Documentation ****************************/

/**
 * SECTION:elliptic
 * @title: elliptic
 * @short_description: Functions to work with elliptic areas
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
