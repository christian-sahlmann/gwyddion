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

#include "gwymacros.h"

#include <string.h>
#include "gwymath.h"

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

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

    return pow10(mag);
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

/* Quickly find median value in an array
 * based on public domain code by Nicolas Devillard */
/**
 * gwy_math_median:
 * @n: Number of items in @array.
 * @array: Array of doubles.  It is modified by this function.  All values are
 *         kept, but their positions in the array change.
 *
 * Finds median of an array of values using Quick select algorithm.
 *
 * Returns: The median value of @array.
 **/
gdouble
gwy_math_median(gsize n, gdouble *array)
{
    gsize lo, hi;
    gsize median;
    gsize middle, ll, hh;

    lo = 0;
    hi = n - 1;
    median = n/2;
    while (TRUE) {
        if (hi <= lo)        /* One element only */
            return array[median];

        if (hi == lo + 1) {  /* Two elements only */
            if (array[lo] > array[hi])
                DSWAP(array[lo], array[hi]);
            return array[median];
        }

        /* Find median of lo, middle and hi items; swap into position lo */
        middle = (lo + hi)/2;
        if (array[middle] > array[hi])
            DSWAP(array[middle], array[hi]);
        if (array[lo] > array[hi])
            DSWAP(array[lo], array[hi]);
        if (array[middle] > array[lo])
            DSWAP(array[middle], array[lo]);

        /* Swap low item (now in position middle) into position (lo+1) */
        DSWAP(array[middle], array[lo + 1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        ll = lo + 1;
        hh = hi;
        while (TRUE) {
            do {
                ll++;
            } while (array[lo] > array[ll]);
            do {
                hh--;
            } while (array[hh] > array[lo]);

            if (hh < ll)
                break;

            DSWAP(array[ll], array[hh]);
        }

        /* Swap middle item (in position lo) back into correct position */
        DSWAP(array[lo], array[hh]);

        /* Re-set active partition */
        if (hh <= median)
            lo = ll;
        if (hh >= median)
            hi = hh - 1;
    }
}

/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size) \
    do \
    { \
        register size_t __size = (size); \
            register char *__a = (a), *__b = (b); \
            do \
            { \
                char __tmp = *__a; \
                    *__a++ = *__b; \
                    *__b++ = __tmp; \
            } while (--__size > 0); \
    } while (0)

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 4

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct
{
    gdouble *lo;
    gdouble *hi;
} stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
/* The stack needs log (total_elements) entries (we could even subtract
   log(MAX_THRESH)).  Since total_elements has type size_t, we get as
   upper bound for log (total_elements):
   bits per byte (CHAR_BIT) * sizeof(size_t).  */
#define STACK_SIZE      (CHAR_BIT * sizeof(size_t))
#define PUSH(low, high) ((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)  ((void) (--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY (stack < top)

/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
   next array partition to sort.  To save time, this maximum amount
   of space required to store an array of SIZE_MAX is allocated on the
   stack.  Assuming a 32-bit (64 bit) integer for size_t, this needs
   only 32 * sizeof(stack_node) == 256 bytes (for 64 bit: 1024 bytes).
   Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
   This reduces the probability of selecting a bad pivot value and
   eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
   insertion sort to order the MAX_THRESH items within each partition.
   This is a big win, since insertion sort is faster for small, mostly
   sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
   stack first, with the algorithm then concentrating on the
   smaller partition.  This *guarantees* no more than log(n)
   stack size is needed (actually O(1) in this case)!  */

void
gwy_math_sort(gsize n,
              gdouble *array)
{
    size_t size = sizeof(gdouble);

    if (n == 0)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        gdouble *lo = array;
        gdouble *hi = lo + (n - 1);
        stack_node stack[STACK_SIZE];
        stack_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            gdouble *left_ptr;
            gdouble *right_ptr;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            gdouble *mid = lo + ((hi - lo) >> 1);

            if (*mid < *lo)
                DSWAP(*mid, *lo);
            if (*hi < *mid)
                DSWAP(*mid, *hi);
            else
                goto jump_over;
            if (*mid < *lo)
                DSWAP(*mid, *lo);
jump_over:;

          left_ptr  = lo + 1;
          right_ptr = hi - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid)
                  left_ptr++;

              while (*mid < *right_ptr)
                  right_ptr--;

              if (left_ptr < right_ptr) {
                  DSWAP(*left_ptr, *right_ptr);
                  if (mid == left_ptr)
                      mid = right_ptr;
                  else if (mid == right_ptr)
                      mid = left_ptr;
                  left_ptr++;
                  right_ptr--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  right_ptr--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((size_t)(right_ptr - lo) <= MAX_THRESH) {
              if ((size_t)(hi - left_ptr) <= MAX_THRESH)
                  /* Ignore both small partitions. */
                  POP(lo, hi);
              else
                  /* Ignore small left partition. */
                  lo = left_ptr;
          }
          else if ((size_t)(hi - left_ptr) <= MAX_THRESH)
              /* Ignore small right partition. */
              hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              PUSH(lo, right_ptr);
              lo = left_ptr;
          }
          else {
              /* Push larger right partition indices. */
              PUSH(left_ptr, hi);
              hi = right_ptr;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        double *const end_ptr = array + (n - 1);
        double *tmp_ptr = array;
        double *thresh = MIN(end_ptr, array + MAX_THRESH);
        register double *run_ptr;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1; run_ptr <= thresh; run_ptr++) {
            if (*run_ptr < *tmp_ptr)
                tmp_ptr = run_ptr;
        }

        if (tmp_ptr != array)
            DSWAP(*tmp_ptr, *array);

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        while ((++run_ptr) <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            while (*run_ptr < *tmp_ptr)
                tmp_ptr--;

            tmp_ptr++;
            if (tmp_ptr != run_ptr) {
                char *trav;

                trav = (gchar*)run_ptr + size;
                while (--trav >= (gchar*)run_ptr)
                {
                    char c = *trav;
                    char *hi, *lo;

                    for (hi = lo = trav; (lo -= size) >= (gchar*)tmp_ptr; hi = lo)
                        *hi = *lo;
                    *hi = c;
                }
            }
        }
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
