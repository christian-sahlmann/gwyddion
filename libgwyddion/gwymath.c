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

#include <string.h>
#include "gwymacros.h"
#include "gwymath.h"

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

/**
 * gwy_math_SI_prefix:
 * @magnitude: A power of 1000.
 *
 * Finds SI prefix corresponding to a given power of 1000.
 *
 * In fact, @magnitude doesn't have to be power of 1000, but then the result
 * is mostly meaningless.
 *
 * Returns: The SI unit prefix corresponding to @magnitude, "?" if @magnitude
 *          is outside of the SI prefix range.  The returned value must be
 *          considered constant and never modified or freed.
 **/
const gchar*
gwy_math_SI_prefix(gdouble magnitude)
{
    static const gchar *positive[] = {
        "", "k", "M", "G", "T", "P", "E", "Z", "Y"
    };
    static const gchar *negative[] = {
        "", "m", "µ", "n", "p", "f", "a", "z", "y"
    };
    static const gchar *unknown = "?";
    gint i;

    i = ROUND(log10(magnitude)/3.0);
    if (i >= 0 && i < (gint)G_N_ELEMENTS(positive))
        return positive[i];
    if (i <= 0 && -i < (gint)G_N_ELEMENTS(negative))
        return negative[-i];
    /* FIXME: the vertical ruler text placing routine can't reasonably
     * break things like 10<sup>-36</sup> to lines */
    g_warning("magnitude %g outside of prefix range.  FIXME!", magnitude);

    return unknown;
}

/**
 * gwy_math_humanize_numbers:
 * @unit: The smallest possible step.
 * @maximum: The maximum possible value.
 * @precision: A location to store printf() precession, if not %NULL.
 *
 * Find a human readable representation for a range of numbers.
 *
 * Returns: The magnitude i.e., a power of 1000.
 **/
gdouble
gwy_math_humanize_numbers(gdouble unit,
                          gdouble maximum,
                          gint *precision)
{
    gdouble lm, lu, mag, range, min;

    lm = log10(maximum);
    lu = log10(unit);
    mag = 3.0*floor((lm + lu)/6.0);
    if (precision) {
        range = lm - lu;
        if (range > 3.0)
            range = (range + 3.0)/2;
        min = lm - range;
        *precision = (min < mag) ? (gint)ceil(mag - min) : 0;
    }

    /* prefer unscaled numbers (mag = 0), if feasible */
    if (mag < 0 && maximum >= 1.0) {
        gwy_debug("killing mag = %f, *precision = %d", mag, *precision);
        *precision += -mag;
        mag = 0;
    }
    else if (mag > 0 && mag <= 3.0 && mag - *precision <= 0) {
        gwy_debug("killing mag = %f, *precision = %d", mag, *precision);
        *precision -= mag;
        mag = 0;
    }

    return exp(G_LN10*mag);
}

/**
 * gwy_math_find_nearest_line:
 * @x: X-coordinate of the point to search.
 * @y: Y-coordinate of the point to search.
 * @d2min: Where to store the squared minimal distance, or %NULL.
 * @n: The number of lines (i.e. @coords has 4@n items).
 * @coords: Line coordinates stored as x00, y00, x01, y01, x10, y10, etc.
 *
 * Find the line from @coords nearest to the point (@x, @y).
 *
 * Returns: The line number. It may return -1 if (@x, @y) doesn't lie
 *          in the orthogonal stripe of any of the lines.
 **/
gint
gwy_math_find_nearest_line(gdouble x, gdouble y,
                           gdouble *d2min,
                           gint n, gdouble *coords)
{
    gint i, m;
    gdouble d2m = G_MAXDOUBLE;

    g_return_val_if_fail(n > 0, d2m);
    g_return_val_if_fail(coords, d2m);

    m = -1;
    for (i = 0; i < n; i++) {
        gdouble xl0 = *(coords++);
        gdouble yl0 = *(coords++);
        gdouble xl1 = *(coords++);
        gdouble yl1 = *(coords++);
        gdouble vx, vy, d;

        vx = yl1 - yl0;
        vy = xl0 - xl1;
        if (vx*(y - yl0) < vy*(x - xl0))
            continue;
        if (vx*(yl1 - y) < vy*(xl1 - x))
            continue;
        if (vx == 0.0 && vy == 0.0)
            continue;
        d = vx*(x - xl0) + vy*(y - yl0);
        d *= d/(vx*vx + vy*vy);
        if (d < d2m) {
            d2m = d;
            m = i;
        }
    }
    if (d2min)
      *d2min = d2m;

    return m;
}

/**
 * gwy_math_find_nearest_point:
 * @x: X-coordinate of the point to search.
 * @y: Y-coordinate of the point to search.
 * @d2min: Where to store the squared minimal distance, or %NULL.
 * @n: The number of points (i.e. @coords has 2@n items).
 * @coords: Point coordinates stored as x0, y0, x1, y1, x2, y2, etc.
 *
 * Find the point from @coords nearest to the point (@x, @y).
 *
 * Returns: The point number.
 **/
gint
gwy_math_find_nearest_point(gdouble x, gdouble y,
                            gdouble *d2min,
                            gint n, gdouble *coords)
{
    gint i, m;
    gdouble d2m = G_MAXDOUBLE;

    g_return_val_if_fail(n > 0, d2m);
    g_return_val_if_fail(coords, d2m);

    m = 0;
    for (i = 0; i < n; i++) {
        gdouble xx = *(coords++);
        gdouble yy = *(coords++);
        gdouble d;

        d = (xx - x)*(xx - x) + (yy - y)*(yy - y);
        if (d < d2m) {
            d2m = d;
            m = i;
        }
    }
    if (d2min)
      *d2min = d2m;

    return m;
}

/**
 * gwy_math_lin_solve:
 * @n: The size of the system.
 * @matrix: The matrix of the system (@n times @n), ordered by row, then
 *          column.
 * @rhs: The right hand side of the sytem.
 * @result: Where the result should be stored.  May be %NULL to allocate
 *          a fresh array for the result.
 *
 * Solve a regular system of linear equations.
 *
 * Returns: The solution (@result if it wasn't %NULL), may be %NULL if the
 *          matrix is singular.
 **/
gdouble*
gwy_math_lin_solve(gint n, const gdouble *matrix,
                   const gdouble *rhs,
                   gdouble *result)
{
    gdouble *m, *r;

    g_return_val_if_fail(n > 0, NULL);
    g_return_val_if_fail(matrix && rhs, NULL);

    m = (gdouble*)g_memdup(matrix, n*n*sizeof(gdouble));
    r = (gdouble*)g_memdup(rhs, n*sizeof(gdouble));
    result = gwy_math_lin_solve_rewrite(n, m, r, result);
    g_free(r);
    g_free(m);

    return result;
}

/**
 * gwy_math_lin_solve_rewrite:
 * @n: The size of the system.
 * @matrix: The matrix of the system (@n times @n), ordered by row, then
 *          column.
 * @rhs: The right hand side of the sytem.
 * @result: Where the result should be stored.  May be %NULL to allocate
 *          a fresh array for the result.
 *
 * Solve a regular system of linear equations.
 *
 * This is a memory-conservative version of gwy_math_lin_solve() overwriting
 * @matrix and @rhs with intermediate results.
 *
 * Returns: The solution (@result if it wasn't %NULL), may be %NULL if the
 *          matrix is singular.
 **/
gdouble*
gwy_math_lin_solve_rewrite(gint n, gdouble *matrix,
                           gdouble *rhs,
                           gdouble *result)
{
    gint *perm;
    gint i, j, jj;

    g_return_val_if_fail(n > 0, NULL);
    g_return_val_if_fail(matrix && rhs, NULL);

    perm = g_new(gint, n);

    /* elimination */
    for (i = 0; i < n; i++) {
        gdouble *row = matrix + i*n;
        gdouble piv = 0;
        gint pivj = 0;

        /* find pivot */
        for (j = 0; j < n; j++) {
            if (fabs(row[j]) > piv) {
                pivj = j;
                piv = fabs(row[j]);
            }
        }
        if (piv == 0.0) {
            g_warning("Singluar matrix");
            g_free(perm);
            return NULL;
        }
        piv = row[pivj];
        perm[i] = pivj;

        /* substract */
        for (j = i+1; j < n; j++) {
            gdouble *jrow = matrix + j*n;
            gdouble q = jrow[pivj]/piv;

            for (jj = 0; jj < n; jj++)
                jrow[jj] -= q*row[jj];

            jrow[pivj] = 0.0;
            rhs[j] -= q*rhs[i];
        }
    }

    /* back substitute */
    if (!result)
        result = g_new(gdouble, n);
    for (i = n-1; i >= 0; i--) {
        gdouble *row = matrix + i*n;
        gdouble x = rhs[i];

        for (j = n-1; j > i; j--)
            x -= result[perm[j]]*row[perm[j]];

        result[perm[i]] = x/row[perm[i]];
    }
    g_free(perm);

    return result;
}

/**
 * gwy_math_fit_polynom:
 * @ndata: The number of items in @xdata, @ydata.
 * @xdata: Independent variable data (of size @ndata).
 * @ydata: Dependent variable data (of size @ndata).
 * @n: The degree of polynom to fit.
 * @coeffs: An array of size @n+1 to store the coefficients to, or %NULL
 *          (a fresh array is allocated then).
 *
 * Fits a polynom through a general (x, y) data set.
 *
 * Returns: The coefficients of the polynom (@coeffs when it was not %NULL,
 *          otherwise a newly allocated array).
 **/
gdouble*
gwy_math_fit_polynom(gint ndata, gdouble *xdata, gdouble *ydata,
                     gint n, gdouble *coeffs)
{
    gdouble *sumx, *m;
    gint i, j;

    g_return_val_if_fail(ndata >= 0, NULL);
    g_return_val_if_fail(n >= 0, NULL);

    sumx = g_new0(gdouble, 2*n+1);

    if (!coeffs)
        coeffs = g_new0(gdouble, n+1);
    else
        memset(coeffs, 0, (n+1)*sizeof(gdouble));

    for (i = 0; i < ndata; i++) {
        gdouble x = xdata[i];
        gdouble y = ydata[i];
        gdouble xp;

        xp = 1.0;
        for (j = 0; j <= n; j++) {
            sumx[j] += xp;
            coeffs[j] += xp*y;
            xp *= x;
        }
        for (j = n+1; j <= 2*n; j++) {
            sumx[j] += xp;
            xp *= x;
        }
    }

    m = g_new(gdouble, (n+1)*(n+2)/2);
    for (i = 0; i <= n; i++) {
        gdouble *row = m + i*(i+1)/2;

        for (j = 0; j <= i; j++)
            row[j] = sumx[i+j];
    }
    if (!gwy_math_choleski_decompose(n+1, m))
        memset(coeffs, 0, (n+1)*sizeof(gdouble));
    else
        gwy_math_choleski_solve(n+1, m, coeffs);

    g_free(m);
    g_free(sumx);

    return coeffs;
}

/**
 * gwy_math_choleski_decompose:
 * @n: The dimension of @a.
 * @matrix: Lower triangular part of a symmetric matrix, stored by rows, i.e.,
 *          matrix = [a_00 a_10 a_11 a_20 a_21 a_22 a_30 ...].
 *
 * Decomposes a symmetric positive definite matrix in place.
 *
 * Returns: Whether the matrix was really positive definite.  If %FALSE,
 *          the decomposition failed and @a does not contain any meaningful
 *          values.
 *
 * Since: 1.6
 **/
gboolean
gwy_math_choleski_decompose(gint dim, gdouble *a)
{
    gint i, j, k;
    gdouble s, r;

    for (k = 0; k < dim; k++) {
        /* diagonal element */
        s = SLi(a, k, k);
        for (i = 0; i < k; i++)
            s -= SLi(a, k, i) * SLi(a, k, i);
        if (s <= 0.0)
            return FALSE;
        SLi(a, k, k) = s = sqrt(s);

        /* nondiagonal elements */
        for (j = k+1; j < dim; j++) {
            r = SLi(a, j, k);
            for (i = 0; i < k; i++)
                r -= SLi(a, k, i) * SLi(a, j, i);
            SLi(a, j, k) = r/s;
        }
    }

    return TRUE;
}

/**
 * gwy_math_choleski_solve:
 * @n: The dimension of @a.
 * @decomp: Lower triangular part of Choleski decomposition as computed
 *          by gwy_math_choleski_decompose().
 * @rhs: Right hand side vector.  Is is modified in place, on return it
 *       contains the solution.
 *
 * Solves a system of linear equations with predecomposed symmetric positive
 * definite matrix @a and right hand side @b.
 *
 * Since: 1.6
 **/
void
gwy_math_choleski_solve(gint dim, gdouble *a, gdouble *b)
{
    gint i, j;

    /* back-substitution with the lower triangular matrix */
    for (j = 0; j < dim; j++) {
        for (i = 0; i < j; i++)
            b[j] -= SLi(a, j, i)*b[i];
        b[j] /= SLi(a, j, j);
    }

    /* back-substitution with the upper triangular matrix */
    for (j = dim-1; j >= 0; j--) {
        for (i = j+1; i < dim; i++)
            b[j] -= SLi(a, i, j)*b[i];
        b[j] /= SLi(a, j, j);
    }
}

/************************** Documentation ****************************/

/**
 * ROUND:
 * @x: A double value.
 *
 * Rounds a number to nearest integer.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
