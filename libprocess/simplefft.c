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
#include <libgwyddion/gwymath.h>
#include <libprocess/simplefft.h>

typedef gdouble (*GwyFFTWindowingFunc)(gint i, gint n);


static gdouble gwy_fft_window_hann     (gint i, gint n);
static gdouble gwy_fft_window_hamming  (gint i, gint n);
static gdouble gwy_fft_window_blackmann(gint i, gint n);
static gdouble gwy_fft_window_lanczos  (gint i, gint n);
static gdouble gwy_fft_window_welch    (gint i, gint n);
static gdouble gwy_fft_window_rect     (gint i, gint n);

/* The order must match GwyWindowingType enum */
static const GwyFFTWindowingFunc windowings[] = {
    NULL,  /* none */
    &gwy_fft_window_hann,
    &gwy_fft_window_hamming,
    &gwy_fft_window_blackmann,
    &gwy_fft_window_lanczos,
    &gwy_fft_window_welch,
    &gwy_fft_window_rect,
};

/**
 * gwy_fft_simple:
 * @dir: Transformation direction.
 * @n: Number of data points.  It must be a power of 2.
 * @istride: Input data stride.
 * @re_in: Real part of input data.
 * @im_in: Imaginary part of input data.
 * @ostride: Output data stride.
 * @re_out: Real part of output data.
 * @im_out: Imaginary part of output data.
 *
 * Performs FST algorithm.
 *
 * This is low-level function used by other FFT functions when no better
 * backend is available.
 *
 * Strides are distances between samples in input and output arrays.  Use 1
 * for normal `dense' arrays.  To use gwy_fft_simple() with interleaved arrays,
 * that is with alternating real and imaginary data, call it with
 * @istride=2, @re_in=@complex_array, @im_in=@complex_array+1 (and similarly
 * for output arrays).
 *
 * The output is symmetrically normalized by square root of @n for both
 * transform directions.  By performing forward and then backward transform,
 * you will obtain the original array (up to rounding errors).
 **/
void
gwy_fft_simple(GwyTransformDirection dir,
               gint n,
               gint istride,
               const gdouble *re_in,
               const gdouble *im_in,
               gint ostride,
               gdouble *re_out,
               gdouble *im_out)
{
    gdouble rc, ic, rt, it, fact;
    gint m, l, i, j, is;
    gdouble imlt;

    imlt = (gint)dir * G_PI;
    fact = 1.0/sqrt(n);
    j = 1;
    for (i = 1; i <= n; i++) {
        if (i <= j) {
            rt = fact * re_in[(i-1)*istride];
            it = fact * im_in[(i-1)*istride];
            re_out[(i-1)*ostride] = fact * re_in[(j-1)*istride];
            im_out[(i-1)*ostride] = fact * im_in[(j-1)*istride];
            re_out[(j-1)*ostride] = rt;
            im_out[(j-1)*ostride] = it;
        }
        m = n >> 1;
        while (j > m && m)
        {
            j -=m; m >>= 1;
        }
        j += m;

    }

    l = 1;
    while (l < n){
        is = l << 1;
        for (m = 1; m <= l; m++) {
            rc = cos(imlt * (m - 1)/l);
            ic = sin(imlt * (m - 1)/l);
            for (i = m; i <= n; i += is) {
                rt = rc*re_out[(i+l-1)*ostride] - ic*im_out[(i+l-1)*ostride];
                it = rc*im_out[(i+l-1)*ostride] + ic*re_out[(i+l-1)*ostride];
                re_out[(i+l-1)*ostride] = re_out[(i-1)*ostride] - rt;
                im_out[(i+l-1)*ostride] = im_out[(i-1)*ostride] - it;
                re_out[(i-1)*ostride] += rt;
                im_out[(i-1)*ostride] += it;
            }
        }
        l = is;
    }
}

static gdouble
gwy_fft_window_hann(gint i, gint n)
{
    return 0.5 - 0.5*(cos(2*G_PI*i/(n-1)));
}

static gdouble
gwy_fft_window_hamming(gint i, gint n)
{
    return 0.54 - 0.46*(cos(2*G_PI*i/(n-1)));
}

static gdouble
gwy_fft_window_blackmann(gint i, gint n)
{
    gdouble n_2 = ((gdouble)n)/2;

    return 0.42 + 0.5*cos(G_PI*(i-n_2)/n_2) + 0.08*cos(2*G_PI*(i-n_2)/n_2);
}

static gdouble
gwy_fft_window_lanczos(gint i, gint n)
{
    gdouble n_2 = ((gdouble)n)/2;

    if (i == n/2)
        return 1;
    return sin(G_PI*(i-n_2)/n_2)/(G_PI*(i-n_2)/n_2);
}

static gdouble
gwy_fft_window_welch(gint i, gint n)
{
    gdouble n_2 = ((gdouble)n)/2;

    return 1 - ((i-n_2)*(i-n_2)/n_2/n_2);
}

static gdouble
gwy_fft_window_rect(gint i, gint n)
{
    gdouble par;

    if (i == 0 || i == (n-1))
        par = 0.5;
    else
        par = 1.0;
    return par;
}

/**
 * gwy_fft_window:
 * @n: Number of data values.
 * @data: Data values.
 * @windowing: Method used for windowing.
 *
 * Multiplies data by given window.
 **/
void
gwy_fft_window(gint n,
               gdouble *data,
               GwyWindowingType windowing)
{
    GwyFFTWindowingFunc window;
    gint i;

    g_return_if_fail(data);
    g_return_if_fail(windowing <= GWY_WINDOWING_RECT);
    window = windowings[windowing];
    if (window) {
        for (i = 0; i < n; i++)
            data[i] *= window(i, n);
    }
}

void
gwy_fft_window_data_field(GwyDataField *dfield,
                          GwyOrientation orientation,
                          GwyWindowingType windowing)
{
    GwyFFTWindowingFunc window;
    gint xres, yres, col, row;
    gdouble *data, q;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(windowing <= GWY_WINDOWING_RECT);

    window = windowings[windowing];
    if (!window)
        return;

    xres = dfield->xres;
    yres = dfield->yres;
    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        for (col = 0; col < xres; col++) {
            q = window(col, xres);
            data = dfield->data + col;
            for (row = 0; row < yres; row++)
                data[row*xres] *= q;
        }
        break;

        case GWY_ORIENTATION_VERTICAL:
        for (row = 0; row < yres; row++) {
            q = window(row, xres);
            data = dfield->data + row*xres;
            for (col = 0; col < xres; col++)
                data[col] *= q;
        }
        break;

        default:
        g_return_if_reached();
        break;
    }

    gwy_data_field_invalidate(dfield);
}

/************************** Documentation ****************************/

/**
 * SECTION:simplefft
 * @title: simplefft
 * @short_description: Simple FFT algorithm
 *
 * The simple one-dimensional FFT algorithm gwy_fft_hum() is used as a fallback
 * by other functions when better implementation (FFTW3) is not available.
 *
 * It works only on data sizes that are powers of 2.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
