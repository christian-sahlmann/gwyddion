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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"

/* Private DataLine functions */
void            _gwy_data_line_initialize        (GwyDataLine *a,
                                                  gint res, gdouble real,
                                                  gboolean nullme);
void            _gwy_data_line_free              (GwyDataLine *a);

/**
 * gwy_data_field_plane_coeffs:
 * @a: A data field
 * @ap: Constant coefficient.
 * @bp: X coefficient.
 * @cp: Y coefficient.
 *
 * Evaluates coefficients of plane fit of data field.
 **/
void
gwy_data_field_plane_coeffs(GwyDataField *a,
                            gdouble *ap, gdouble *bp, gdouble *cp)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble nx = a->xres;
    gdouble ny = a->yres;
    gdouble bx, by;
    gdouble *pdata;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));

    sumxi = (nx-1)/2;
    sumxixi = (2*nx-1)*(nx-1)/6;
    sumyi = (ny-1)/2;
    sumyiyi = (2*ny-1)*(ny-1)/6;

    pdata = a->data;
    for (i = 0; i < a->xres*a->yres; i++) {
        sumsi += *pdata;
        sumsixi += *pdata * (i%a->xres);
        sumsiyi += *pdata * (i/a->xres);
        *pdata++;
    }
    sumsi /= nx*ny;
    sumsixi /= nx*ny;
    sumsiyi /= nx*ny;

    bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
    by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);
    if (bp)
        *bp = bx*nx/a->xreal;
    if (cp)
        *cp = by*ny/a->yreal;
    if (ap)
        *ap = sumsi - bx*sumxi - by*sumyi;
}

/**
 * gwy_data_field_area_fit_plane:
 * @dfield: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @pa: Where constant coefficient should be stored (or %NULL).
 * @pbx: Where x plane coefficient should be stored (or %NULL).
 * @pby: Where y plane coefficient should be stored (or %NULL).
 *
 * Fits a plane through a rectangular part of a data field.
 *
 * Since: 1.2.
 **/
void
gwy_data_field_area_fit_plane(GwyDataField *dfield,
                              gint col, gint row, gint width, gint height,
                              gdouble *pa, gdouble *pbx, gdouble *pby)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble a, bx, by;
    gdouble *datapos;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    /* try to return something reasonable even in degenerate cases */
    if (!width || !height)
        a = bx = by = 0.0;
    else if (height == 1 && width == 1) {
        a = dfield->data[row*dfield->xres + col];
        bx = by = 0.0;
    }
    else {
        sumxi = (width - 1.0)/2;
        sumyi = (height - 1.0)/2;
        sumxixi = (2.0*width - 1.0)*(width - 1.0)/6;
        sumyiyi = (2.0*height - 1.0)*(height - 1.0)/6;

        datapos = dfield->data + row*dfield->xres + col;
        for (i = 0; i < height; i++) {
            gdouble *drow = datapos + i*dfield->xres;

            for (j = 0; j < width; j++) {
                sumsi += drow[j];
                sumsixi += drow[j]*j;
                sumsiyi += drow[j]*i;
            }
        }
        sumsi /= width*height;
        sumsixi /= width*height;
        sumsiyi /= width*height;

        if (width == 1)
            bx = 0.0;
        else
            bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);

        if (height == 1)
            by = 0.0;
        else
            by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);

        a = sumsi - bx*sumxi - by*sumyi;
        bx *= width/dfield->xreal;
        by *= height/dfield->yreal;
    }

    if (pa)
        *pa = a;
    if (pbx)
        *pbx = bx;
    if (pby)
        *pby = by;
}

/**
 * gwy_data_field_plane_level:
 * @a: A data field
 * @ap: Constant coefficient.
 * @bp: X coefficient.
 * @cp: Y coefficient.
 *
 * Plane leveling.
 **/
void
gwy_data_field_plane_level(GwyDataField *a, gdouble ap, gdouble bp, gdouble cp)
{
    gint i, j;
    gdouble bpix = bp/a->xres*a->xreal;
    gdouble cpix = cp/a->yres*a->yreal;

    for (i = 0; i < a->yres; i++) {
        gdouble *row = a->data + i*a->xres;
        gdouble rb = ap + cpix*i;

        for (j = 0; j < a->xres; j++, row++)
            *row -= rb + bpix*j;
    }
}

/**
 * gwy_data_field_plane_rotate:
 * @a: A data field
 * @xangle: rotation angle in x direction (rotation along y axis)
 * @yangle: rotation angle in y direction (rotation along x axis)
 * @interpolation: interpolation type
 *
 * Performs rotation of plane along x and y axis.
 **/
void
gwy_data_field_plane_rotate(GwyDataField *a, gdouble xangle, gdouble yangle,
                            GwyInterpolationType interpolation)
{
    int k;
    GwyDataLine l;

    if (xangle != 0) {
        _gwy_data_line_initialize(&l, a->xres, a->xreal, 0);
        for (k = 0; k < a->yres; k++) {
            gwy_data_field_get_row(a, &l, k);
            gwy_data_line_line_rotate(&l, -xangle, interpolation);
            gwy_data_field_set_row(a, &l, k);
        }
        _gwy_data_line_free(&l);
    }


    if (yangle != 0) {
        _gwy_data_line_initialize(&l, a->yres, a->yreal, 0);
        for (k = 0; k < a->xres; k++) {
            gwy_data_field_get_column(a, &l, k);
            gwy_data_line_line_rotate(&l, -yangle, interpolation);
            gwy_data_field_set_column(a, &l, k);
        }
        _gwy_data_line_free(&l);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
