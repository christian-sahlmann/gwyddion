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

/* INTERPOLATION: New (not applicable). */

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
        *pdata++;
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
 * as in gwy_data_field_area_fit_plane(), counting indices from area top left
 * corner.
 **/
void
gwy_data_field_area_fit_plane(GwyDataField *data_field,
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

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    /* try to return something reasonable even in degenerate cases */
    if (!width || !height)
        a = bx = by = 0.0;
    else if (height == 1 && width == 1) {
        a = data_field->data[row*data_field->xres + col];
        bx = by = 0.0;
    }
    else {
        sumxi = (width - 1.0)/2;
        sumyi = (height - 1.0)/2;
        sumxixi = (2.0*width - 1.0)*(width - 1.0)/6;
        sumyiyi = (2.0*height - 1.0)*(height - 1.0)/6;

        datapos = data_field->data + row*data_field->xres + col;
        for (i = 0; i < height; i++) {
            gdouble *drow = datapos + i*data_field->xres;

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
 * @interpolation: Interpolation type.
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
            gwy_data_line_line_rotate(l, -xangle, interpolation);
            gwy_data_field_set_row(data_field, l, k);
        }
        g_object_unref(l);
    }

    if (yangle != 0) {
        l = gwy_data_line_new(data_field->yres, data_field->yreal, FALSE);
        for (k = 0; k < data_field->xres; k++) {
            gwy_data_field_get_column(data_field, l, k);
            gwy_data_line_line_rotate(l, -yangle, interpolation);
            gwy_data_field_set_column(data_field, l, k);
        }
        g_object_unref(l);
    }

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_area_fit_polynom:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @col_degree: Degree of polynom to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynom to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then).
 *
 * Fits a two-dimensional polynom to a rectangular part of a data field.
 *
 * The coefficients are stored by row into @coeffs, like data in a datafield.
 * Row index is y-degree, column index is x-degree.
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
                         && width > 0 && height > 0
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
        memset(coeffs, 0, size*sizeof(gdouble));

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
        memset(coeffs, 0, size*sizeof(gdouble));
    else
        gwy_math_choleski_solve(size, m, coeffs);

    g_free(m);
    g_free(sums);

    return coeffs;
}

/**
 * gwy_data_field_fit_polynom:
 * @data_field: A data field.
 * @col_degree: Degree of polynom to fit column-wise (x-coordinate).
 * @row_degree: Degree of polynom to fit row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) to store the
 *          coefficients to, or %NULL (a fresh array is allocated then),
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Fits a two-dimensional polynom to a data field.
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
 * @col_degree: Degree of polynom to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynom to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Subtracts a two-dimensional polynom from a rectangular part of a data field.
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
 * @col_degree: Degree of polynom to subtract column-wise (x-coordinate).
 * @row_degree: Degree of polynom to subtract row-wise (y-coordinate).
 * @coeffs: An array of size (@row_degree+1)*(@col_degree+1) with coefficients,
 *          see gwy_data_field_area_fit_polynom() for details.
 *
 * Subtracts a two-dimensional polynom from a data field.
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
    gdouble xreal, yreal, qx, qy;
    gint xres, yres, ri, i, j, ii, jj;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(coeffs, NULL);
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
            shift = ifrom - (i + row);
            sumxy += shift*sumx;
            sumyy += shift*(2*sumy + n*shift);
            sumzy += shift*sumz;
            sumy += n*shift;

            shift = jfrom - (j + col);
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
 * GwyPlaneFitQuantity:
 * @GWY_PLANE_FIT_A: Constant coefficient (mean value).
 * @GWY_PLANE_FIT_BX: Linear coefficient in x, if x in in pixel coordinates.
 * @GWY_PLANE_FIT_BY: Linear coefficient in y, if y is in pixel coordinates.
 * @GWY_PLANE_FIT_ANGLE: Slope orientation in (x,y) plane (in radians).
 * @GWY_PLANE_FIT_SLOPE: Absolute slope value (that is sqrt(bx*bx + by*by)).
 * @GWY_PLANE_FIT_S0: Residual sum of squares.
 * @GWY_PLANE_FIT_S0_REDUCED: Slope-reduced residual sum of squares.
 *
 * Quantity that can be requested from gwy_data_field_area_fit_local_planes()
 * et al.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
