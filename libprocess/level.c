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
#include <libprocess/datafield.h>
#include <libprocess/level.h>

/**
 * gwy_data_field_fit_plane:
 * @data_field: A data field.
 * @pa: Where constant coefficient should be stored (or %NULL).
 * @pbx: Where x plane coefficient should be stored (or %NULL).
 * @pby: Where y plane coefficient should be stored (or %NULL).
 *
 * Fits a plane through a data field.
 *
 * The coefficients can be used for plane leveling using relation
 * data[i] := data[i] - (pa + pby*i + pbx*j);
 **/
void
gwy_data_field_fit_plane(GwyDataField *data_field,
                         gdouble *pa, gdouble *pbx, gdouble *pby)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble nx, ny;
    gdouble bx, by;
    gdouble *pdata;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    nx = data_field->xres;
    ny = data_field->yres;

    sumxi = (nx-1)/2;
    sumxixi = (2*nx-1)*(nx-1)/6;
    sumyi = (ny-1)/2;
    sumyiyi = (2*ny-1)*(ny-1)/6;

    pdata = data_field->data;
    for (i = 0; i < data_field->xres*data_field->yres; i++) {
        sumsi += *pdata;
        sumsixi += *pdata * (i%data_field->xres);
        sumsiyi += *pdata * (i/data_field->xres);
        pdata++;
    }
    sumsi /= nx*ny;
    sumsixi /= nx*ny;
    sumsiyi /= nx*ny;

    bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
    by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);
    if (pbx)
        *pbx = bx;
    if (pby)
        *pby = by;
    if (pa)
        *pa = sumsi - bx*sumxi - by*sumyi;
}

/**
 * gwy_data_field_area_fit_plane:
 * @data_field: A data field
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
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
 * The coefficients can be used for plane leveling using the same relation
 * as in gwy_data_field_fit_plane(), counting indices from area top left
 * corner.
 **/
void
gwy_data_field_area_fit_plane(GwyDataField *data_field,
                              GwyDataField *mask,
                              gint col, gint row, gint width, gint height,
                              gdouble *pa, gdouble *pbx, gdouble *pby)
{
    gdouble sumx, sumy, sumz, sumxx, sumyy, sumxy, sumxz, sumyz;
    gdouble alpha1, alpha2, alpha3, beta1, beta2, gamma1;
    gdouble det;
    gdouble a, b, c;
    gint xres, yres;
    gdouble x, y, z;
    gdouble n;
    gdouble *datapos = NULL, *maskpos = NULL;
    gdouble *drow = NULL, *mrow = NULL;
    gint i, j;
    gboolean skip;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    if (mask) {
        g_return_if_fail(GWY_IS_DATA_FIELD(mask));
        g_return_if_fail(mask->xres == data_field->xres
                         && mask->yres == data_field->yres);
    }

    xres = data_field->xres;
    yres = data_field->yres;
    n = 0;

    /* try to return something reasonable even in degenerate cases */
    if (!width || !height)
        a = b = c = 0.0;
    else if (height == 1 && width == 1) {
        c = data_field->data[row*xres + col];
        a = b = 0.0;
    }
    else {
        sumx = sumy = sumz = sumxx = sumyy = sumxy = sumxz = sumyz = 0;
        datapos = data_field->data + row*xres + col;
        if (mask)
            maskpos = mask->data + row*xres + col;
        for (i = 0; i < height; i++) {
            drow = datapos + i*xres;
            if (mask)
                mrow = maskpos + i*xres;
            for (j = 0; j < width; j++) {
                skip = FALSE;
                if (mask)
                    if (mrow[j] == 0.0)
                        skip = TRUE;

                if (!skip) {
                    x = j;
                    y = i;
                    z = drow[j];

                    sumx += x;
                    sumy += y;
                    sumz += z;
                    sumxx += x*x;
                    sumyy += y*y;
                    sumxy += x*y;
                    sumxz += x*z;
                    sumyz += y*z;
                    n++;
                }
            }
        }

        det = (n*sumxx*sumyy) + (2*sumx*sumxy*sumy) - (sumx*sumx*sumyy)
                -(sumy*sumy*sumxx) - (n*sumxy*sumxy);

        /* try to return something reasonable in case of singularity */
        if (det == 0.0)
            a = b = c = 0.0;
        else
        {
            det = 1.0/det;

            alpha1 = (n*sumyy) - (sumy*sumy);
            alpha2 = (n*sumxx) - (sumx*sumx);
            alpha3 = (sumxx*sumyy) - (sumxy*sumxy);
            beta1 = (sumx*sumy) - (n*sumxy);
            beta2 = (sumx*sumxy) - (sumxx*sumy);
            gamma1 = (sumxy*sumy) - (sumx*sumyy);

            a = det*(alpha1*sumxz + beta1*sumyz + gamma1*sumz);
            b = det*(beta1*sumxz + alpha2*sumyz + beta2*sumz);
            c = det*(gamma1*sumxz + beta2*sumyz + alpha3*sumz);
        }
    }

    if (pbx)
        *pbx = a;
    if (pby)
        *pby = b;
    if (pa)
        *pa = c;
}

/**
 * gwy_data_field_plane_level:
 * @data_field: A data field.
 * @a: Constant coefficient.
 * @bx: X plane coefficient.
 * @by: Y plane coefficient.
 *
 * Subtracts plane from a data field.
 *
 * See gwy_data_field_fit_plane() for details.
 **/
void
gwy_data_field_plane_level(GwyDataField *data_field,
                           gdouble a, gdouble bx, gdouble by)
{
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    for (i = 0; i < data_field->yres; i++) {
        gdouble *row = data_field->data + i*data_field->xres;
        gdouble rb = a + by*i;

        for (j = 0; j < data_field->xres; j++, row++)
            *row -= rb + bx*j;
    }

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_plane_rotate:
 * @data_field: A data field.
 * @xangle: Rotation angle in x direction (rotation along y axis, in radians).
 * @yangle: Rotation angle in y direction (rotation along x axis, in radians).
 * @interpolation: Interpolation type (can be only of two-point type).
 *
 * Performs rotation of plane along x and y axis.
 **/
void
gwy_data_field_plane_rotate(GwyDataField *data_field,
                            gdouble xangle,
                            gdouble yangle,
                            GwyInterpolationType interpolation)
{
    int k;
    GwyDataLine *l;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    if (xangle != 0) {
        l = gwy_data_line_new(data_field->xres, data_field->xreal, FALSE);
        for (k = 0; k < data_field->yres; k++) {
            gwy_data_field_get_row(data_field, l, k);
            gwy_data_line_rotate(l, -xangle, interpolation);
            gwy_data_field_set_row(data_field, l, k);
        }
        g_object_unref(l);
    }

    if (yangle != 0) {
        l = gwy_data_line_new(data_field->yres, data_field->yreal, FALSE);
        for (k = 0; k < data_field->xres; k++) {
            gwy_data_field_get_column(data_field, l, k);
            gwy_data_line_rotate(l, -yangle, interpolation);
            gwy_data_field_set_column(data_field, l, k);
        }
        g_object_unref(l);
    }

    gwy_data_field_invalidate(data_field);
}

#if 0
void
gwy_data_field_plane_true_rotate(GwyDataField *data_field,
                                 gdouble xangle,
                                 gdouble yangle,
                                 GwyInterpolationType interpolation)
{
    gdouble diag, dx, dy, phi, phi0, theta, tx, ty;
    gint xres, yres, txres, tyres, xbw, ybw, i;
    gdouble *data, *tdata;
    GwyDataField *tmp;

    if (xangle == 0 || yangle == 0) {
        gwy_data_field_plane_rotate(data_field, xangle, yangle, interpolation);
        return;
    }

    xres = data_field->xres;
    yres = data_field->yres;
    data = data_field->data;

    dx = tan(xangle);
    dy = tan(yangle);
    phi = atan2(dy, dx);
    theta = atan(hypot(dx, dy));
    phi0 = atan2(yres, xres);
    diag = hypot(xres, yres);
    tx = MAX(fabs(cos(-phi + phi0)), fabs(cos(-phi - phi0)));
    ty = MAX(fabs(sin(-phi + phi0)), fabs(sin(-phi - phi0)));
    txres = ((guint)GWY_ROUND(diag*tx + 2));
    tyres = ((guint)GWY_ROUND(diag*ty + 2));
    /* Keep parity to make the rotation less fuzzy */
    xbw = (txres - xres + 1)/2;
    if (xres + 2*xbw != txres)
        txres++;
    ybw = (tyres - yres + 1)/2;
    if (yres + 2*ybw != tyres)
        tyres++;

    /* Rotate to a temporary data field extended with border pixels */
    tmp = gwy_data_field_new(txres, tyres, 1.0, 1.0, FALSE);
    tdata = tmp->data;
    /* Copy */
    gwy_data_field_area_copy(data_field, tmp, 0, 0, xres, yres, xbw, ybw);
    /* Corners */
    gwy_data_field_area_fill(tmp, 0, 0, xbw, ybw,
                             data[0]);
    gwy_data_field_area_fill(tmp, xres + xbw, 0, xbw, ybw,
                             data[xres-1]);
    gwy_data_field_area_fill(tmp, 0, yres + ybw, xbw, ybw,
                             data[xres*(yres - 1)]);
    gwy_data_field_area_fill(tmp, xres + xbw, yres + ybw, xbw, ybw,
                             data[xres*yres - 1]);
    /* Sides */
    for (i = 0; i < ybw; i++)
        memcpy(tdata + i*txres + xbw, data,
               xres*sizeof(gdouble));
    for (i = 0; i < ybw; i++)
        memcpy(tdata + (yres + ybw + i)*txres + xbw, data + xres*(yres - 1),
               xres*sizeof(gdouble));
    for (i = 0; i < yres; i++) {
        gwy_data_field_area_fill(tmp, 0, ybw + i, xbw, 1,
                                 data[i*xres]);
        gwy_data_field_area_fill(tmp, xres + xbw, ybw + i, xbw, 1,
                                 data[i*xres + xres - 1]);
    }

    /* Rotate in xy to make the space rotation along y axis */
    gwy_data_field_rotate(tmp, -phi, interpolation);
    /* XXX: Still, individual gwy_data_line_rotate() can resample differently,
     * causing incompatible rows in the image.  And we cannot get the
     * resampling information from gwy_data_line_rotate(). */
    gwy_data_field_plane_rotate(tmp, theta, 0, GWY_INTERPOLATION_LINEAR);
    /* TODO:
     * recalculate xres
     * make samples square again
     */
    gwy_data_field_rotate(tmp, phi, interpolation);
    /* XXX: xbw is no longer correct border */
    gwy_data_field_area_copy(tmp, data_field, xbw, ybw, xres, yres, 0, 0);

    g_object_unref(tmp);
}
#endif

/**
 * gwy_data_field_fit_lines:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @degree: Fitted polynomial degree.
 * @exclude: If %TRUE, outside of area selected by @ulcol, @ulrow, @brcol,
 *           @brrow will be used for polynomial coefficients computation,
 *           instead of inside.
 * @orientation: Line orientation.
 *
 * Independently levels profiles on each row/column in a data field.
 *
 * Lines that have no intersection with area selected by @ulcol, @ulrow,
 * @brcol, @brrow are always leveled as a whole.  Lines that have intersection
 * with selected area, are leveled using polynomial coefficients computed only
 * from data inside (or outside for @exclude = %TRUE) the area.
 **/
void
gwy_data_field_fit_lines(GwyDataField *data_field,
                         gint col, gint row,
                         gint width, gint height,
                         gint degree,
                         gboolean exclude,
                         GwyOrientation orientation)
{

    gint i, j, xres, yres, res;
    gdouble real, coefs[4];
    GwyDataLine *hlp, *xdata = NULL, *ydata = NULL;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    xres = data_field->xres;
    yres = data_field->yres;
    res = (orientation == GWY_ORIENTATION_HORIZONTAL) ? xres : yres;
    real = (orientation == GWY_ORIENTATION_HORIZONTAL)
           ? data_field->xreal : data_field->yreal;
    hlp = gwy_data_line_new(res, real, FALSE);
    if (exclude) {
        xdata = gwy_data_line_new(res, real, FALSE);
        ydata = gwy_data_line_new(res, real, FALSE);
    }

    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        if (exclude) {
            for (i = j = 0; i < xres; i++) {
                if (i < col || i >= col + width)
                    xdata->data[j++] = i;
            }
        }

        for (i = 0; i < yres; i++) {
            gwy_data_field_get_row(data_field, hlp, i);
            if (i >= row && i < row + height) {
                if (exclude) {
                    memcpy(ydata->data, hlp->data, col*sizeof(gdouble));
                    memcpy(ydata->data + col, hlp->data + col + width,
                           (xres - col - width)*sizeof(gdouble));
                    gwy_math_fit_polynom(xres - width,
                                         xdata->data, ydata->data, degree,
                                         coefs);
                }
                else
                    gwy_data_line_part_fit_polynom(hlp, degree, coefs,
                                                   col, col + width);
            }
            else
                gwy_data_line_fit_polynom(hlp, degree, coefs);
            gwy_data_line_subtract_polynom(hlp, degree, coefs);
            gwy_data_field_set_row(data_field, hlp, i);
        }
    }
    else if (orientation == GWY_ORIENTATION_VERTICAL) {
        if (exclude) {
            for (i = j = 0; i < yres; i++) {
                if (i < row || i >= row + height)
                    xdata->data[j++] = i;
            }
        }

        for (i = 0; i < xres; i++) {
            gwy_data_field_get_column(data_field, hlp, i);
            if (i >= col && i < col + width) {
                if (exclude) {
                    memcpy(ydata->data, hlp->data, row*sizeof(gdouble));
                    memcpy(ydata->data + row, hlp->data + row + height,
                           (yres - row - height)*sizeof(gdouble));
                    gwy_math_fit_polynom(yres - height,
                                         xdata->data, ydata->data, degree,
                                         coefs);
                }
                else
                    gwy_data_line_part_fit_polynom(hlp, degree, coefs,
                                                   row, row + height);
            }
            else
                gwy_data_line_fit_polynom(hlp, degree, coefs);
            gwy_data_line_subtract_polynom(hlp, degree, coefs);
            gwy_data_field_set_column(data_field, hlp, i);
        }
    }
    g_object_unref(hlp);
    gwy_object_unref(xdata);
    gwy_object_unref(ydata);
}

/**
 * gwy_data_field_area_fit_polynom:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits a two-dimensional polynomial to a rectangular part of a data field.
 *
 * The coefficients are stored by row into @coeffs, like data in a datafield.
 * Row index is y-degree, column index is x-degree.
 *
 * Note naive x^n y^m polynomial fitting is numerically unstable, therefore
 * this method works only up to @col_degree = @row_degree = 6.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_area_fit_polynom(GwyDataField *data_field,
                                gint col, gint row,
                                gint width, gint height,
                                gint col_degree, gint row_degree,
                                gdouble *coeffs)
{
    gint r, c, i, j, size, xres, yres;
    gdouble *data, *sums, *m;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(row_degree >= 0 && col_degree >= 0, NULL);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width > col_degree && height > row_degree
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         NULL);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    size = (row_degree+1)*(col_degree+1);
    if (!coeffs)
        coeffs = g_new0(gdouble, size);
    else
        gwy_clear(coeffs, size);

    sums = g_new0(gdouble, (2*row_degree+1)*(2*col_degree+1));
    for (r = row; r < row + height; r++) {
        for (c = col; c < col + width; c++) {
            gdouble ry = 1.0;
            gdouble z = data[r*xres + c];

            for (i = 0; i <= 2*row_degree; i++) {
                gdouble cx = 1.0;

                for (j = 0; j <= 2*col_degree; j++) {
                    sums[i*(2*col_degree+1) + j] += cx*ry;
                    cx *= c;
                }
                ry *= r;
            }

            ry = 1.0;
            for (i = 0; i <= row_degree; i++) {
                gdouble cx = 1.0;

                for (j = 0; j <= col_degree; j++) {
                    coeffs[i*(col_degree+1) + j] += cx*ry*z;
                    cx *= c;
                }
                ry *= r;
            }
        }
    }

    m = g_new(gdouble, size*(size+1)/2);
    for (i = 0; i < size; i++) {
        gdouble *mrow = m + i*(i+1)/2;

        for (j = 0; j <= i; j++) {
            gint pow_x, pow_y;

            pow_x = i % (col_degree+1) + j % (col_degree+1);
            pow_y = i/(col_degree+1) + j/(col_degree+1);
            mrow[j] = sums[pow_y*(2*col_degree+1) + pow_x];
        }
    }

    if (!gwy_math_choleski_decompose(size, m))
        gwy_clear(coeffs, size);
    else
        gwy_math_choleski_solve(size, m, coeffs);

    g_free(m);
    g_free(sums);

    return coeffs;
}

/**
 * gwy_data_field_fit_polynom:
 * @data_field: A data field.
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then),
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Fits a two-dimensional polynomial to a data field.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_fit_polynom(GwyDataField *data_field,
                           gint col_degree, gint row_degree,
                           gdouble *coeffs)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return gwy_data_field_area_fit_polynom(data_field, 0, 0,
                                           data_field->xres, data_field->yres,
                                           col_degree, row_degree, coeffs);
}

/**
 * gwy_data_field_area_subtract_polynom:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynomial to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Subtracts a two-dimensional polynomial from a rectangular part of a data
 * field.
 **/
void
gwy_data_field_area_subtract_polynom(GwyDataField *data_field,
                                     gint col, gint row,
                                     gint width, gint height,
                                     gint col_degree, gint row_degree,
                                     const gdouble *coeffs)
{
    gint r, c, i, j, size, xres, yres;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(coeffs);
    g_return_if_fail(row_degree >= 0 && col_degree >= 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    size = (row_degree+1)*(col_degree+1);

    for (r = row; r < row + height; r++) {
        for (c = col; c < col + width; c++) {
            gdouble ry = 1.0;
            gdouble z = data[r*xres + c];

            for (i = 0; i <= row_degree; i++) {
                gdouble cx = 1.0;

                for (j = 0; j <= col_degree; j++) {
                    /* FIXME: this is wrong, use Horner schema */
                    z -= coeffs[i*(col_degree+1) + j]*cx*ry;
                    cx *= c;
                }
                ry *= r;
            }

            data[r*xres + c] = z;
        }
    }

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_subtract_polynom:
 * @data_field: A data field.
 * @col_degree: Degree of polynomial to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Subtracts a two-dimensional polynomial from a data field.
 **/
void
gwy_data_field_subtract_polynom(GwyDataField *data_field,
                                gint col_degree, gint row_degree,
                                const gdouble *coeffs)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_subtract_polynom(data_field,
                                         0, 0,
                                         data_field->xres, data_field->yres,
                                         col_degree, row_degree, coeffs);
}

/* Calculate values of Legendre polynomials from 0 to @n in @x. */
static void
legendre_all(gdouble x,
             guint n,
             gdouble *p)
{
    guint m;

    p[0] = 1.0;
    if (n == 0)
        return;
    p[1] = x;
    if (n == 1)
        return;

    for (m = 2; m <= n; m++)
        p[m] = (x*(2*m - 1)*p[m-1] - (m - 1)*p[m-2])/m;
}

/**
 * gwy_data_field_area_fit_legendre:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits two-dimensional Legendre polynomial to a rectangular part of a data
 * field.
 *
 * The @col_degree and @row_degree parameters limit the maximum powers of x and
 * y exactly as if simple powers were fitted, therefore if you do not intend to
 * interpret contents of @coeffs youself, the only difference is that this
 * method is much more numerically stable.
 *
 * The coefficients are organized exactly like in
 * gwy_data_field_area_fit_polynom(), but they are not coefficients of
 * x^n y^m, instead they are coefficients of P_n(x) P_m(x), where P are
 * Legendre polynomials.  The polynomials are evaluated in coordinates where
 * first row (column) corresponds to -1.0, and the last row (column) to 1.0.
 *
 * Note the polynomials are normal Legendre polynomials that are not exactly
 * orthogonal on a discrete point set (if their degrees are equal mod 2).
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_area_fit_legendre(GwyDataField *data_field,
                                 gint col, gint row,
                                 gint width, gint height,
                                 gint col_degree, gint row_degree,
                                 gdouble *coeffs)
{
    gint r, c, i, j, size, maxsize, xres, yres, col_n, row_n;
    gint isize, jsize, thissize;
    gdouble *data, *m, *pmx, *pmy, *sumsx, *sumsy, *rhs;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(row_degree >= 0 && col_degree >= 0, NULL);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width > col_degree && height > row_degree
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         NULL);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    col_n = col_degree + 1;
    row_n = row_degree + 1;
    size = col_n*row_n;
    /* The maximum necessary matrix size (order), it is approximately four
     * times smaller thanks to separation of even and odd polynomials */
    maxsize = ((col_n + 1)/2)*((row_n + 1)/2);
    if (!coeffs)
        coeffs = g_new0(gdouble, size);
    else
        gwy_clear(coeffs, size);

    sumsx = g_new0(gdouble, col_n*col_n);
    sumsy = g_new0(gdouble, row_n*row_n);
    rhs = g_new(gdouble, maxsize);
    m = g_new(gdouble, MAX(maxsize*(maxsize + 1)/2, col_n + row_n));
    /* pmx, pmy and m are not needed at the same time, reuse it */
    pmx = m;
    pmy = m + col_n;

    /* Calculate <P_m(x) P_n(y) z(x,y)> (normalized to complete area) */
    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, row_degree, pmy);
        for (c = 0; c < width; c++) {
            gdouble z = data[(row + r)*xres + (col + c)];

            legendre_all(2*c/(width - 1.0) - 1.0, col_degree, pmx);
            for (i = 0; i < row_n; i++) {
                for (j = 0; j < col_n; j++)
                    coeffs[i*col_n + j] += z*pmx[j]*pmy[i];
            }
        }
    }

    /* Calculate <P_m(x) P_a(x)> (normalized to single row).
     * 3/4 of these values are zeroes, but it only takes O(width) time. */
    for (c = 0; c < width; c++) {
        legendre_all(2*c/(width - 1.0) - 1.0, col_degree, pmx);
        for (i = 0; i < col_n; i++) {
            for (j = 0; j < col_n; j++)
                sumsx[i*col_n + j] += pmx[i]*pmx[j];
        }
    }

    /* Calculate <P_n(y) P_b(y)> (normalized to single column)
     * 3/4 of these values are zeroes, but it only takes O(height) time. */
    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, row_degree, pmy);
        for (i = 0; i < row_n; i++) {
            for (j = 0; j < row_n; j++)
                sumsy[i*row_n + j] += pmy[i]*pmy[j];
        }
    }

    /* (Even, Even) */
    isize = (row_n + 1)/2;
    jsize = (col_n + 1)/2;
    thissize = jsize*isize;
    /* This is always true */
    if (thissize) {
        /* Construct the submatrix */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize);
            gdouble *mrow = m + i*(i + 1)/2;

            for (j = 0; j <= i; j++) {
                gint jx = 2*(j % jsize);
                gint jy = 2*(j/jsize);

                mrow[j] = sumsx[ix*col_n + jx]*sumsy[iy*row_n + jy];
            }
        }
        /* Construct the subrhs */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize);

            rhs[i] = coeffs[iy*col_n + ix];
        }
        /* Solve */
        if (!gwy_math_choleski_decompose(thissize, m)) {
            gwy_clear(coeffs, size);
            goto fail;
        }
        gwy_math_choleski_solve(thissize, m, rhs);
        /* Copy back */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize);

            coeffs[iy*col_n + ix] = rhs[i];
        }
    }

    /* (Even, Odd) */
    isize = (row_n + 1)/2;
    jsize = col_n/2;
    thissize = jsize*isize;
    if (thissize) {
        /* Construct the submatrix */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize);
            gdouble *mrow = m + i*(i + 1)/2;

            for (j = 0; j <= i; j++) {
                gint jx = 2*(j % jsize) + 1;
                gint jy = 2*(j/jsize);

                mrow[j] = sumsx[ix*col_n + jx]*sumsy[iy*row_n + jy];
            }
        }
        /* Construct the subrhs */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize);

            rhs[i] = coeffs[iy*col_n + ix];
        }
        /* Solve */
        if (!gwy_math_choleski_decompose(thissize, m)) {
            gwy_clear(coeffs, size);
            goto fail;
        }
        gwy_math_choleski_solve(thissize, m, rhs);
        /* Copy back */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize);

            coeffs[iy*col_n + ix] = rhs[i];
        }
    }

    /* (Odd, Even) */
    isize = row_n/2;
    jsize = (col_n + 1)/2;
    thissize = jsize*isize;
    if (thissize) {
        /* Construct the submatrix */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize) + 1;
            gdouble *mrow = m + i*(i + 1)/2;

            for (j = 0; j <= i; j++) {
                gint jx = 2*(j % jsize);
                gint jy = 2*(j/jsize) + 1;

                mrow[j] = sumsx[ix*col_n + jx]*sumsy[iy*row_n + jy];
            }
        }
        /* Construct the subrhs */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize) + 1;

            rhs[i] = coeffs[iy*col_n + ix];
        }
        /* Solve */
        if (!gwy_math_choleski_decompose(thissize, m)) {
            gwy_clear(coeffs, size);
            goto fail;
        }
        gwy_math_choleski_solve(thissize, m, rhs);
        /* Copy back */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize);
            gint iy = 2*(i/jsize) + 1;

            coeffs[iy*col_n + ix] = rhs[i];
        }
    }

    /* (Odd, Odd) */
    isize = row_n/2;
    jsize = col_n/2;
    thissize = jsize*isize;
    if (thissize) {
        /* Construct the submatrix */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize) + 1;
            gdouble *mrow = m + i*(i + 1)/2;

            for (j = 0; j <= i; j++) {
                gint jx = 2*(j % jsize) + 1;
                gint jy = 2*(j/jsize) + 1;

                mrow[j] = sumsx[ix*col_n + jx]*sumsy[iy*row_n + jy];
            }
        }
        /* Construct the subrhs */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize) + 1;

            rhs[i] = coeffs[iy*col_n + ix];
        }
        /* Solve */
        if (!gwy_math_choleski_decompose(thissize, m)) {
            gwy_clear(coeffs, size);
            goto fail;
        }
        gwy_math_choleski_solve(thissize, m, rhs);
        /* Copy back */
        for (i = 0; i < thissize; i++) {
            gint ix = 2*(i % jsize) + 1;
            gint iy = 2*(i/jsize) + 1;

            coeffs[iy*col_n + ix] = rhs[i];
        }
    }

fail:
    g_free(m);
    g_free(rhs);
    g_free(sumsx);
    g_free(sumsy);

    return coeffs;
}

/**
 * gwy_data_field_fit_legendre:
 * @data_field: A data field.
 * @col_degree: Degree of polynomial to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits two-dimensional Legendre polynomial to a data field.
 *
 * See gwy_data_field_area_fit_legendre() for details.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_fit_legendre(GwyDataField *data_field,
                            gint col_degree, gint row_degree,
                            gdouble *coeffs)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return gwy_data_field_area_fit_legendre(data_field, 0, 0,
                                            data_field->xres, data_field->yres,
                                            col_degree, row_degree, coeffs);
}

/**
 * gwy_data_field_area_subtract_legendre:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynomial to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_legendre() for details.
 *
 * Subtracts a two-dimensional Legendre polynomial fit from a rectangular part
 * of a data field.
 *
 * Due to the transform of coordinates to [-1,1] x [-1,1], this method can be
 * used on an area of dimensions different than the area the coefficients were
 * calculated for.
 **/
void
gwy_data_field_area_subtract_legendre(GwyDataField *data_field,
                                      gint col, gint row,
                                      gint width, gint height,
                                      gint col_degree, gint row_degree,
                                      const gdouble *coeffs)
{
    gint r, c, i, j, size, xres, yres, col_n, row_n;
    gdouble *data, *pmx, *pmy;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(coeffs);
    g_return_if_fail(row_degree >= 0 && col_degree >= 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    col_n = col_degree + 1;
    row_n = row_degree + 1;
    size = col_n*row_n;

    pmx = g_new0(gdouble, col_n + row_n);
    pmy = pmx + col_n;

    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, row_degree, pmy);
        for (c = 0; c < width; c++) {
            gdouble z = data[(row + r)*xres + (col + c)];

            legendre_all(2*c/(width - 1.0) - 1.0, col_degree, pmx);
            for (i = 0; i < row_n; i++) {
                for (j = 0; j < col_n; j++)
                    z -= coeffs[i*col_n + j]*pmx[j]*pmy[i];
            }

            data[(row + r)*xres + (col + c)] = z;
        }
    }

    g_free(pmx);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_subtract_legendre:
 * @data_field: A data field.
 * @col_degree: Degree of polynomial to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynomial to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_legendre() for details.
 *
 * Subtracts a two-dimensional Legendre polynomial fit from a data field.
 **/
void
gwy_data_field_subtract_legendre(GwyDataField *data_field,
                                 gint col_degree, gint row_degree,
                                 const gdouble *coeffs)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_subtract_legendre(data_field,
                                          0, 0,
                                          data_field->xres, data_field->yres,
                                          col_degree, row_degree, coeffs);
}

/**
 * gwy_data_field_area_fit_poly_max:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @max_degree: Maximum total polynomial degree, that is the maximum of m+n
 *              in x^n y^m terms.
 * @coeffs: An array of size (@max_degree+1)*(@max_degree+2)/2 to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits two-dimensional polynomial with limited total degree to a rectangular
 * part of a data field.
 *
 * See gwy_data_field_area_fit_legendre() for description.  This function
 * differs by limiting the total maximum degree, while
 * gwy_data_field_area_fit_legendre() limits the maximum degrees in horizontal
 * and vertical directions independently.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_area_fit_poly_max(GwyDataField *data_field,
                                 gint col, gint row,
                                 gint width, gint height,
                                 gint max_degree,
                                 gdouble *coeffs)
{
    gint r, c, i, j, size, xres, yres, degree_n;
    gint ix, jx, iy, jy;
    gdouble *data, *m, *pmx, *pmy, *sumsx, *sumsy;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(max_degree >= 0, NULL);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         NULL);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    degree_n = max_degree + 1;
    size = degree_n*(degree_n + 1)/2;
    g_return_val_if_fail(width*height > size, NULL);
    /* The maximum necessary matrix size (order), it is approximately four
     * times smaller thanks to separation of even and odd polynomials */
    if (!coeffs)
        coeffs = g_new0(gdouble, size);
    else
        gwy_clear(coeffs, size);

    sumsx = g_new0(gdouble, degree_n*degree_n);
    sumsy = g_new0(gdouble, degree_n*degree_n);
    m = g_new(gdouble, MAX(size*(size + 1)/2, 2*degree_n));
    /* pmx, pmy and m are not needed at the same time, reuse it */
    pmx = m;
    pmy = m + degree_n;

    /* Calculate <P_m(x) P_n(y) z(x,y)> (normalized to complete area) */
    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, max_degree, pmy);
        for (c = 0; c < width; c++) {
            gdouble z = data[(row + r)*xres + (col + c)];

            legendre_all(2*c/(width - 1.0) - 1.0, max_degree, pmx);
            for (i = 0; i < degree_n; i++) {
                for (j = 0; j < degree_n - i; j++)
                    coeffs[i*(2*degree_n + 1 - i)/2 + j] += z*pmx[j]*pmy[i];
            }
        }
    }

    /* Calculate <P_m(x) P_a(x)> (normalized to single row).
     * 3/4 of these values are zeroes, but it only takes O(width) time. */
    for (c = 0; c < width; c++) {
        legendre_all(2*c/(width - 1.0) - 1.0, max_degree, pmx);
        for (i = 0; i < degree_n; i++) {
            for (j = 0; j < degree_n; j++)
                sumsx[i*degree_n + j] += pmx[i]*pmx[j];
        }
    }

    /* Calculate <P_n(y) P_b(y)> (normalized to single column)
     * 3/4 of these values are zeroes, but it only takes O(height) time. */
    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, max_degree, pmy);
        for (i = 0; i < degree_n; i++) {
            for (j = 0; j < degree_n; j++)
                sumsy[i*degree_n + j] += pmy[i]*pmy[j];
        }
    }

    /* Construct the matrix */
    for (iy = 0; iy < degree_n; iy++) {
        for (jy = 0; jy < degree_n - iy; jy++) {
            gdouble *mrow;

            i = iy*(2*degree_n + 1 - iy)/2 + jy;
            mrow = m + i*(i + 1)/2;
            for (ix = 0; ix < degree_n; ix++) {
                for (jx = 0; jx < degree_n - ix; jx++) {
                    j = ix*(2*degree_n + 1 - ix)/2 + jx;
                    /* It is easier to go through all the coeffs and ignore
                     * the upper right triangle than to construct conditions
                     * directly for jy, jy, etc. */
                    if (j > i)
                        continue;
                    mrow[j] = sumsx[jy*degree_n + jx]*sumsy[iy*degree_n + ix];
                }
            }
        }
    }
    /* Solve */
    if (!gwy_math_choleski_decompose(size, m)) {
        gwy_clear(coeffs, size);
        goto fail;
    }
    gwy_math_choleski_solve(size, m, coeffs);

fail:
    g_free(m);
    g_free(sumsx);
    g_free(sumsy);

    return coeffs;
}

/**
 * gwy_data_field_fit_poly_max:
 * @data_field: A data field.
 * @max_degree: Maximum total polynomial degree, that is the maximum of m+n
 *              in x^n y^m terms.
 * @coeffs: An array of size (@max_degree+1)*(@max_degree+2)/2 to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits two-dimensional polynomial with limited total degree to a data field.
 *
 * See gwy_data_field_area_fit_poly_max() for details.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 **/
gdouble*
gwy_data_field_fit_poly_max(GwyDataField *data_field,
                            gint max_degree,
                            gdouble *coeffs)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return gwy_data_field_area_fit_poly_max(data_field, 0, 0,
                                            data_field->xres, data_field->yres,
                                            max_degree, coeffs);
}

/**
 * gwy_data_field_area_subtract_poly_max:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @max_degree: Maximum total polynomial degree, that is the maximum of m+n
 *              in x^n y^m terms.
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+2)/2 with
 *          coefficients, see gwy_data_field_area_fit_poly_max() for details.
 *
 * Subtracts a two-dimensional polynomial with limited total degree from a
 * rectangular part of a data field.
 *
 * Due to the transform of coordinates to [-1,1] x [-1,1], this method can be
 * used on an area of dimensions different than the area the coefficients were
 * calculated for.
 **/
void
gwy_data_field_area_subtract_poly_max(GwyDataField *data_field,
                                      gint col, gint row,
                                      gint width, gint height,
                                      gint max_degree,
                                      const gdouble *coeffs)
{
    gint r, c, i, j, size, xres, yres, degree_n;
    gdouble *data, *pmx, *pmy;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(coeffs);
    g_return_if_fail(max_degree >= 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;
    degree_n = max_degree + 1;
    size = degree_n*(degree_n + 1)/2;

    pmx = g_new0(gdouble, 2*degree_n);
    pmy = pmx + degree_n;

    for (r = 0; r < height; r++) {
        legendre_all(2*r/(height - 1.0) - 1.0, max_degree, pmy);
        for (c = 0; c < width; c++) {
            gdouble z = data[(row + r)*xres + (col + c)];

            legendre_all(2*c/(width - 1.0) - 1.0, max_degree, pmx);
            for (i = 0; i < degree_n; i++) {
                for (j = 0; j < degree_n - i; j++)
                    z -= coeffs[i*(2*degree_n + 1 - i)/2 + j]*pmx[j]*pmy[i];
            }

            data[(row + r)*xres + (col + c)] = z;
        }
    }

    g_free(pmx);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_subtract_poly_max:
 * @data_field: A data field.
 * @max_degree: Maximum total polynomial degree, that is the maximum of m+n
 *              in x^n y^m terms.
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+2)/2 with
 *          coefficients, see gwy_data_field_area_fit_poly_max() for details.
 *
 * Subtracts a two-dimensional polynomial with limited total degree from
 * a data field.
 **/
void
gwy_data_field_subtract_poly_max(GwyDataField *data_field,
                                 gint max_degree,
                                 const gdouble *coeffs)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_subtract_poly_max(data_field,
                                          0, 0,
                                          data_field->xres, data_field->yres,
                                          max_degree, coeffs);
}

/* Calculate integer powers by repeated squaring */
static gdouble
pow_int(gdouble x, guint n)
{
    gdouble v = 1.0;

    while (n) {
        if (n & 1)
            v *= x;
        x *= x;
        n >>= 1;
    }

    return v;
}

/**
 * gwy_data_field_area_fit_poly:
 * @data_field: A data field.
 * @mask_field: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nterms: The number of polynomial terms to take into account (twice the
 *          number of items in @term_powers).
 * @term_powers: Array of size 2*@nterms describing the terms to fit.  Each
 *               terms is described by a couple of powers (powerx, powery).
 * @exclude: Interpret values @w in the mask as 1.0-@w.
 * @coeffs: Array of size @nterms to store the coefficients to, or %NULL to
 *          allocate a new array.
 *
 * Fit a given set of polynomial terms to a rectangular part of a data field.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 *
 * Since: 2.11
 **/
gdouble*
gwy_data_field_area_fit_poly(GwyDataField *data_field,
                             GwyDataField *mask_field,
                             gint col, gint row,
                             gint width, gint height,
                             gint nterms,
                             const gint *term_powers,
                             gboolean exclude,
                             gdouble *coeffs)
{
    const gdouble *data, *mask;
    gint xres, yres, r, c, i, j, k;
    gdouble *m, *p;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(nterms >= 0, NULL);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         NULL);
    if (mask_field) {
        g_return_val_if_fail(GWY_IS_DATA_FIELD(mask_field), NULL);
        g_return_val_if_fail(mask_field->xres == data_field->xres
                             && mask_field->yres == data_field->yres, NULL);
    }

    if (!nterms)
        return coeffs;

    data = data_field->data;
    mask = mask_field ? mask_field->data : NULL;
    xres = data_field->xres;
    yres = data_field->yres;

    if (!coeffs)
        coeffs = g_new0(gdouble, nterms);

    p = g_new(gdouble, nterms);
    m = g_new0(gdouble, nterms*(nterms + 1)/2);

    for (r = 0; r < height; r++) {
        gdouble y = 2*r/(height - 1.0) - 1.0;
        for (c = 0; c < width; c++) {
            gdouble x = 2*c/(width - 1.0) - 1.0;
            gdouble z = data[(row + r)*xres + (col + c)];
            gdouble w = mask[(row + r)*xres + (col + c)];

            if (exclude)
                w = 1.0-w;

            if (w <= 0.0)
                continue;
            if (w >= 1.0)
                w = 1.0;

            for (i = 0; i < nterms; i++) {
                p[i] = pow_int(x, term_powers[2*i])
                       * pow_int(y, term_powers[2*i + 1]);
            }

            k = 0;
            for (i = 0; i < nterms; i++) {
                for (j = 0; j <= i; j++)
                    m[k++] += w*p[i]*p[j];
                coeffs[i] += z*w*p[i];
            }
        }
    }

    if (!gwy_math_choleski_decompose(nterms, m))
        gwy_clear(coeffs, nterms);
    else
        gwy_math_choleski_solve(nterms, m, coeffs);

    g_free(p);
    g_free(m);

    return coeffs;
}

/**
 * gwy_data_field_fit_poly:
 * @data_field: A data field.
 * @mask_field: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @nterms: The number of polynomial terms to take into account (twice the
 *          number of items in @term_powers).
 * @term_powers: Array of size 2*@nterms describing the terms to fit.  Each
 *               terms is described by a couple of powers (powerx, powery).
 * @exclude: Interpret values @w in the mask as 1.0-@w.
 * @coeffs: Array of size @nterms to store the coefficients to, or %NULL to
 *          allocate a new array.
 *
 * Fit a given set of polynomial terms to a data field.
 *
 * Returns: Either @coeffs if it was not %NULL, or a newly allocated array
 *          with coefficients.
 *
 * Since: 2.11
 **/
gdouble*
gwy_data_field_fit_poly(GwyDataField *data_field,
                        GwyDataField *mask_field,
                        gint nterms,
                        const gint *term_powers,
                        gboolean exclude,
                        gdouble *coeffs)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return gwy_data_field_area_fit_poly(data_field, mask_field,
                                        0, 0,
                                        data_field->xres, data_field->yres,
                                        nterms, term_powers, exclude,
                                        coeffs);
}

/**
 * gwy_data_field_area_subtract_poly:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nterms: The number of polynomial terms to take into account (twice the
 *          number of items in @term_powers).
 * @term_powers: Array of size 2*@nterms describing the fitted terms.  Each
 *               terms is described by a couple of powers (powerx, powery).
 * @coeffs: Array of size @nterms to store with the coefficients.
 *
 * Subtract a given set of polynomial terms from a rectangular part of a data
 * field.
 *
 * Since: 2.11
 **/
void
gwy_data_field_area_subtract_poly(GwyDataField *data_field,
                                  gint col, gint row,
                                  gint width, gint height,
                                  gint nterms,
                                  const gint *term_powers,
                                  const gdouble *coeffs)
{
    gdouble *data;
    gint xres, yres, r, c, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(nterms >= 0);
    g_return_if_fail(coeffs);
    g_return_if_fail(col >= 0 && row >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (!nterms)
        return;

    data = data_field->data;
    xres = data_field->xres;
    yres = data_field->yres;

    for (r = 0; r < height; r++) {
        gdouble y = 2*r/(height - 1.0) - 1.0;
        for (c = 0; c < width; c++) {
            gdouble x = 2*c/(width - 1.0) - 1.0;
            gdouble z = data[(row + r)*xres + (col + c)];

            for (i = 0; i < nterms; i++) {
                z -= coeffs[i] * pow_int(x, term_powers[2*i])
                     * pow_int(y, term_powers[2*i + 1]);
            }

            data[(row + r)*xres + (col + c)] = z;
        }
    }

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_subtract_poly:
 * @data_field: A data field.
 * @nterms: The number of polynomial terms to take into account (twice the
 *          number of items in @term_powers).
 * @term_powers: Array of size 2*@nterms describing the fitter terms.  Each
 *               terms is described by a couple of powers (powerx, powery).
 * @coeffs: Array of size @nterms to store with the coefficients.
 *
 * Subtract a given set of polynomial terms from a data field.
 *
 * Since: 2.11
 **/
void
gwy_data_field_subtract_poly(GwyDataField *data_field,
                             gint nterms,
                             const gint *term_powers,
                             const gdouble *coeffs)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_subtract_poly(data_field,
                                      0, 0,
                                      data_field->xres, data_field->yres,
                                      nterms, term_powers, coeffs);
}

/**
 * gwy_data_field_area_fit_local_planes:
 * @data_field: A data field.
 * @size: Neighbourhood size (must be at least 2).  It is centered around
 *        each pixel, unless @size is even when it sticks to the right.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nresults: The number of requested quantities.
 * @types: The types of requested quantities.
 * @results: An array to store quantities to, may be %NULL to allocate a new
 *           one which must be freed by caller then.  If any item is %NULL,
 *           a new data field is allocated for it, existing data fields
 *           are resized to @width x @height.
 *
 * Fits a plane through neighbourhood of each sample in a rectangular part
 * of a data field.
 *
 * The sample is always in the origin of its local (x,y) coordinate system,
 * even if the neighbourhood is not centered about it (e.g. because sample
 * is on the edge of data field).  Z-coordinate is however not centered,
 * that is @GWY_PLANE_FIT_A is normal mean value.
 *
 * Returns: An array of data fields with requested quantities, that is
 *          @results unless it was %NULL and a new array was allocated.
 **/
GwyDataField**
gwy_data_field_area_fit_local_planes(GwyDataField *data_field,
                                     gint size,
                                     gint col, gint row,
                                     gint width, gint height,
                                     gint nresults,
                                     const GwyPlaneFitQuantity *types,
                                     GwyDataField **results)
{
    gdouble coeffs[GWY_PLANE_FIT_S0_REDUCED + 1];
    gdouble xreal, yreal, qx, qy, asymshfit;
    gint xres, yres, ri, i, j, ii, jj;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(size > 1, NULL);
    g_return_val_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres, NULL);
    if (!nresults)
        return NULL;
    g_return_val_if_fail(types, NULL);
    for (ri = 0; ri < nresults; ri++) {
        g_return_val_if_fail(types[ri] >= GWY_PLANE_FIT_A
                             && types[ri] <= GWY_PLANE_FIT_S0_REDUCED,
                             NULL);
        g_return_val_if_fail(!results
                             || !results[ri]
                             || GWY_IS_DATA_FIELD(results[ri]),
                             NULL);
    }
    if (!results)
        results = g_new0(GwyDataField*, nresults);

    /* Allocate output data fields or fix their dimensions */
    xres = data_field->xres;
    yres = data_field->yres;
    qx = data_field->xreal/xres;
    qy = data_field->yreal/yres;
    xreal = qx*width;
    yreal = qy*height;
    for (ri = 0; ri < nresults; ri++) {
        if (!results[ri])
            results[ri] = gwy_data_field_new(width, height, xreal, yreal,
                                             FALSE);
        else {
            gwy_data_field_resample(results[ri], width, height,
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_set_xreal(results[ri], xreal);
            gwy_data_field_set_yreal(results[ri], yreal);
        }
    }

    /* Fit local planes */
    asymshfit = (1 - size % 2)/2.0;
    for (i = 0; i < height; i++) {
        gint ifrom = MAX(0, i + row - (size-1)/2);
        gint ito = MIN(yres-1, i + row + size/2);

        /* Prevent fitting plane through just one pixel on bottom edge when
         * size == 2 */
        if (G_UNLIKELY(ifrom == ito) && ifrom)
            ifrom--;

        for (j = 0; j < width; j++) {
            gint jfrom = MAX(0, j + col - (size-1)/2);
            gint jto = MIN(xres-1, j + col + size/2);
            gdouble *drect;
            gdouble sumz, sumzx, sumzy, sumzz, sumx, sumy, sumxx, sumxy, sumyy;
            gdouble n, bx, by, s0, s0r, det, shift;

            /* Prevent fitting plane through just one pixel on right edge when
             * size == 2 */
            if (G_UNLIKELY(jfrom == jto) && jfrom)
                jfrom--;

            drect = data_field->data + ifrom*xres + jfrom;
            /* Compute sums with origin in top left corner */
            sumz = sumzx = sumzy = sumzz = 0.0;
            for (ii = 0; ii <= ito - ifrom; ii++) {
                gdouble *drow = drect + xres*ii;

                for (jj = 0; jj <= jto - jfrom; jj++) {
                    sumz += drow[jj];
                    sumzx += drow[jj]*jj;
                    sumzy += drow[jj]*ii;
                    sumzz += drow[jj]*drow[jj];
                }
            }
            n = (ito - ifrom + 1)*(jto - jfrom + 1);
            sumx = n*(jto - jfrom)/2.0;
            sumy = n*(ito - ifrom)/2.0;
            sumxx = sumx*(2*(jto - jfrom) + 1)/3.0;
            sumyy = sumy*(2*(ito - ifrom) + 1)/3.0;
            sumxy = sumx*sumy/n;

            /* Move origin to pixel, including in z coordinate, remembering
             * average z value in shift */
            shift = ifrom - (i + row + asymshfit);
            sumxy += shift*sumx;
            sumyy += shift*(2*sumy + n*shift);
            sumzy += shift*sumz;
            sumy += n*shift;

            shift = jfrom - (j + col + asymshfit);
            sumxx += shift*(2*sumx + n*shift);
            sumxy += shift*sumy;
            sumzx += shift*sumz;
            sumx += n*shift;

            shift = -sumz/n;
            sumzx += shift*sumx;
            sumzy += shift*sumy;
            sumzz += shift*(2*sumz + n*shift);
            /* sumz = 0.0;  unused */

            /* Compute coefficients */
            det = sumxx*sumyy - sumxy*sumxy;
            bx = (sumzx*sumyy - sumxy*sumzy)/det;
            by = (sumzy*sumxx - sumxy*sumzx)/det;
            s0 = sumzz - bx*sumzx - by*sumzy;
            s0r = s0/(1.0 + bx*bx/qx/qx + by*by/qy/qy);

            coeffs[GWY_PLANE_FIT_A] = -shift;
            coeffs[GWY_PLANE_FIT_BX] = bx;
            coeffs[GWY_PLANE_FIT_BY] = by;
            coeffs[GWY_PLANE_FIT_ANGLE] = atan2(by, bx);
            coeffs[GWY_PLANE_FIT_SLOPE] = hypot(bx, by);
            coeffs[GWY_PLANE_FIT_S0] = s0;
            coeffs[GWY_PLANE_FIT_S0_REDUCED] = s0r;

            for (ri = 0; ri < nresults; ri++)
                results[ri]->data[width*i + j] = coeffs[types[ri]];
        }
    }

    for (ri = 0; ri < nresults; ri++)
        gwy_data_field_invalidate(results[ri]);

    return results;
}

/**
 * gwy_data_field_fit_local_planes:
 * @data_field: A data field.
 * @size: Neighbourhood size.
 * @nresults: The number of requested quantities.
 * @types: The types of requested quantities.
 * @results: An array to store quantities to.
 *
 * Fits a plane through neighbourhood of each sample in a data field.
 *
 * See gwy_data_field_area_fit_local_planes() for details.
 *
 * Returns: An array of data fields with requested quantities.
 **/
GwyDataField**
gwy_data_field_fit_local_planes(GwyDataField *data_field,
                                gint size,
                                gint nresults,
                                const GwyPlaneFitQuantity *types,
                                GwyDataField **results)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return gwy_data_field_area_fit_local_planes(data_field, size,
                                                0, 0,
                                                data_field->xres,
                                                data_field->yres,
                                                nresults, types, results);
}

/**
 * gwy_data_field_area_local_plane_quantity:
 * @data_field: A data field.
 * @size: Neighbourhood size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @type: The type of requested quantity.
 * @result: A data field to store result to, or %NULL to allocate a new one.
 *
 * Convenience function to get just one quantity from
 * gwy_data_field_area_fit_local_planes().
 *
 * Returns: @result if it isn't %NULL, otherwise a newly allocated data field.
 **/
GwyDataField*
gwy_data_field_area_local_plane_quantity(GwyDataField *data_field,
                                         gint size,
                                         gint col, gint row,
                                         gint width, gint height,
                                         GwyPlaneFitQuantity type,
                                         GwyDataField *result)
{
    gwy_data_field_area_fit_local_planes(data_field, size,
                                         col, row, width, height,
                                         1, &type, &result);
    return result;
}

/**
 * gwy_data_field_local_plane_quantity:
 * @data_field: A data field.
 * @size: Neighbourhood size.
 * @type: The type of requested quantity.
 * @result: A data field to store result to, or %NULL to allocate a new one.
 *
 * Convenience function to get just one quantity from
 * gwy_data_field_fit_local_planes().
 *
 * Returns: @result if it isn't %NULL, otherwise a newly allocated data field.
 **/
GwyDataField*
gwy_data_field_local_plane_quantity(GwyDataField *data_field,
                                    gint size,
                                    GwyPlaneFitQuantity type,
                                    GwyDataField *result)
{
    gwy_data_field_fit_local_planes(data_field, size, 1, &type, &result);

    return result;
}

/************************** Documentation ****************************/

/**
 * SECTION:level
 * @title: level
 * @short_description: Leveling and background removal
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
