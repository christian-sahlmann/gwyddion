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

/**
 * gwy_fft_hum:
 * @dir: Transformation direction.
 * @re_in: Real part of input data.
 * @im_in: Imaginary part of input data.
 * @re_out: Real part of output data.
 * @im_out: Imaginary part of output data.
 * @n: Number of data points.  It must be a power of 2.
 *
 * Performs FST algorithm.
 **/
void
gwy_fft_simple(GwyTransformDirection dir,
            const gdouble *re_in,
            const gdouble *im_in,
            gdouble *re_out,
            gdouble *im_out,
            gint n,
            gint stride)
{
    gdouble rc, ic, rt, it, fact;
    gint m, l, i, j, is;
    gdouble imlt;

    imlt = (gint)dir * G_PI;
    fact = 1.0/sqrt(n);
    j = 1;
    for (i = 1; i <= n; i++) {
        if (i <= j) {
            rt = fact * re_in[(i-1)*stride];
            it = fact * im_in[(i-1)*stride];
            re_out[(i-1)*stride] = fact * re_in[(j-1)*stride];
            im_out[(i-1)*stride] = fact * im_in[(j-1)*stride];
            re_out[(j-1)*stride] = rt;
            im_out[(j-1)*stride] = it;
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
                rt = rc*re_out[(i+l-1)*stride] - ic*im_out[(i+l-1)*stride];
                it = rc*im_out[(i+l-1)*stride] + ic*re_out[(i+l-1)*stride];
                re_out[(i+l-1)*stride] = re_out[(i-1)*stride] - rt;
                im_out[(i+l-1)*stride] = im_out[(i-1)*stride] - it;
                re_out[(i-1)*stride] += rt;
                im_out[(i-1)*stride] += it;
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
    return 0.54 - 0.46*(cos(2*G_PI*i/n));
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

static void
gwy_fft_mult(gdouble *data, gint n, GwyFFTWindowingFunc window)
{
    gint i;

    for (i = 0; i < n; i++)
        data[i] *= window(i, n);
}

/**
 * gwy_fft_window:
 * @data: Data values.
 * @n: Number of data values.
 * @windowing: Method used for windowing.
 *
 * Multiplies data by given window.
 **/
void
gwy_fft_window(gdouble *data,
               gint n,
               GwyWindowingType windowing)
{
    /* The order must match GwyWindowingType enum */
    GwyFFTWindowingFunc windowings[] = {
        NULL,  /* none */
        gwy_fft_window_hann,
        gwy_fft_window_hamming,
        gwy_fft_window_blackmann,
        gwy_fft_window_lanczos,
        gwy_fft_window_welch,
        gwy_fft_window_rect,
    };

    g_return_if_fail(data);
    g_return_if_fail(windowing <= GWY_WINDOWING_RECT);
    if (windowings[windowing])
        gwy_fft_mult(data, n, windowings[windowing]);
}

void 
gwy_fft_window_datafield(GwyDataField *dfield,
                         GtkOrientation orientation,
                         GwyWindowingType windowing)
{
    gint res, xres, yres, col, row, i;
    gdouble *table, *data;
    
    
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        res = xres;
    else
        res = yres;

    table = (gdouble *)g_try_malloc(res*sizeof(gdouble));
    g_assert(table);

    for (i = 0; i<res; i++) table[i] = 1;
    
    gwy_fft_window(table, res, windowing);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
       for (col = 0; col < xres; col++)
       {
           *data += xres;
           for (row = 0; row < yres; row++)
               data[row] *= table[row];
       }
    }
    else
    {
       for (col = 0; col < xres; col++)
       {
           for (row = 0; row < yres; row++)
               data[col] *= table[col];
       }
    }
         
    g_free(table);
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
