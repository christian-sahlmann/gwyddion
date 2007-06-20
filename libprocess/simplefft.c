/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
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

#define C10_1 .80901699437494742410229341718281905886015458990288
#define C10_2 .30901699437494742410229341718281905886015458990289
#define S10_1 .58778525229247312916870595463907276859765243764313
#define S10_2 .95105651629515357211643933337938214340569863412574

#define C5_1 C10_2
#define C5_2 -C10_1
#define S5_1 S10_2
#define S5_2 S10_1

#define C7_1 .62348980185873353052500488400423981063227473089641
#define C7_2 -.22252093395631440428890256449679475946635556876452
#define C7_3 -.90096886790241912623610231950744505116591916213184
#define S7_1 .78183148246802980870844452667405775023233451870867
#define S7_2 .97492791218182360701813168299393121723278580062000
#define S7_3 .43388373911755812047576833284835875460999072778748

#ifdef HAVE_SINCOS
#define _gwy_sincos sincos
#else
static inline void
_gwy_sincos(gdouble x, gdouble *s, gdouble *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

typedef void (*ButterflyFunc)(guint gn,
                              guint stride, gdouble *re, gdouble *im);

typedef gdouble (*GwyFFTWindowingFunc)(gint i, gint n);

static gdouble gwy_fft_window_hann     (gint i, gint n);
static gdouble gwy_fft_window_hamming  (gint i, gint n);
static gdouble gwy_fft_window_blackman (gint i, gint n);
static gdouble gwy_fft_window_lanczos  (gint i, gint n);
static gdouble gwy_fft_window_welch    (gint i, gint n);
static gdouble gwy_fft_window_rect     (gint i, gint n);
static gdouble gwy_fft_window_nuttall  (gint i, gint n);
static gdouble gwy_fft_window_flat_top (gint i, gint n);
static gdouble gwy_fft_window_kaiser25 (gint i, gint n);

/* The order must match GwyWindowingType enum */
static const GwyFFTWindowingFunc windowings[] = {
    NULL,  /* none */
    &gwy_fft_window_hann,
    &gwy_fft_window_hamming,
    &gwy_fft_window_blackman,
    &gwy_fft_window_lanczos,
    &gwy_fft_window_welch,
    &gwy_fft_window_rect,
    &gwy_fft_window_nuttall,
    &gwy_fft_window_flat_top,
    &gwy_fft_window_kaiser25,
};

static void
shuffle_and_twiddle(guint gn, guint gm, guint p,
                    guint istride, const gdouble *in_re, const gdouble *in_im,
                    guint ostride, gdouble *out_re, gdouble *out_im)
{
    gdouble *ff_re, *ff_im;
    guint m, k2, n1;

    /* k2 == 0, twiddle factors are 1 */
    for (m = 0; m < gn/gm; m++) {
        const gdouble *inb_re = in_re + istride*m;
        const gdouble *inb_im = in_im + istride*m;
        gdouble *outb_re = out_re + ostride*m;
        gdouble *outb_im = out_im + ostride*m;

        for (n1 = 0; n1 < p; n1++) {
            guint li = gn/gm*istride*n1;
            guint lo = gn/p*ostride*n1;

            outb_re[lo] = inb_re[li];
            outb_im[lo] = inb_im[li];
        }
    }
    if (gm == p)
        return;

    /* Other twiddle factors have to be calculated,
       but for n1 == 0 they are always 1 */
    ff_re = g_newa(gdouble, p);
    ff_im = g_newa(gdouble, p);
    for (k2 = 1; k2 < gm/p; k2++) {
        for (n1 = 1; n1 < p; n1++)
            _gwy_sincos(2.0*G_PI*n1*k2/gm, ff_im + n1, ff_re + n1);
        for (m = 0; m < gn/gm; m++) {
            const gdouble *inb_re = in_re + istride*(m + gn*p/gm*k2);
            const gdouble *inb_im = in_im + istride*(m + gn*p/gm*k2);
            gdouble *outb_re = out_re + ostride*(m + gn/gm*k2);
            gdouble *outb_im = out_im + ostride*(m + gn/gm*k2);

            outb_re[0] = inb_re[0];
            outb_im[0] = inb_im[0];
            for (n1 = 1; n1 < p; n1++) {
                guint li = gn/gm*istride*n1;
                guint lo = gn/p*ostride*n1;

                outb_re[lo] = ff_re[n1]*inb_re[li] - ff_im[n1]*inb_im[li];
                outb_im[lo] = ff_re[n1]*inb_im[li] + ff_im[n1]*inb_re[li];
            }
        }
    }
}

static void
pass2(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 2;
    if (stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble z;

            z = rem[0] - rem[gn];
            rem[0] += rem[gn];
            rem[gn] = z;

            z = imm[0] - imm[gn];
            imm[0] += imm[gn];
            imm[gn] = z;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble z;

            z = re[stride*m] - re[stride*(gn + m)];
            re[stride*m] += re[stride*(gn + m)];
            re[stride*(gn + m)] = z;

            z = im[stride*m] - im[stride*(gn + m)];
            im[stride*m] += im[stride*(gn + m)];
            im[stride*(gn + m)] = z;
        }
    }
}

static void
pass3(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 3;
    if (stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble z1re, z1im, z2re, z2im;

            z1re = rem[gn] + rem[2*gn];
            z1im = imm[gn] + imm[2*gn];
            /* Multiplication by i */
            z2re = (imm[2*gn] - imm[gn])*0.5*GWY_SQRT3;
            z2im = (rem[gn] - rem[2*gn])*0.5*GWY_SQRT3;
            rem[2*gn] = rem[0] - (z2re + 0.5*z1re);
            imm[2*gn] = imm[0] - (z2im + 0.5*z1im);
            rem[gn] = rem[0] + (z2re - 0.5*z1re);
            imm[gn] = imm[0] + (z2im - 0.5*z1im);
            rem[0] += z1re;
            imm[0] += z1im;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble z1re, z1im, z2re, z2im;

            z1re = re[stride*(gn + m)] + re[stride*(2*gn + m)];
            z1im = im[stride*(gn + m)] + im[stride*(2*gn + m)];
            /* Multiplication by i */
            z2re = (im[stride*(2*gn + m)] - im[stride*(gn + m)])*0.5*GWY_SQRT3;
            z2im = (re[stride*(gn + m)] - re[stride*(2*gn + m)])*0.5*GWY_SQRT3;
            re[stride*(2*gn + m)] = re[stride*m] - (z2re + 0.5*z1re);
            im[stride*(2*gn + m)] = im[stride*m] - (z2im + 0.5*z1im);
            re[stride*(gn + m)] = re[stride*m] + (z2re - 0.5*z1re);
            im[stride*(gn + m)] = im[stride*m] + (z2im - 0.5*z1im);
            re[stride*m] += z1re;
            im[stride*m] += z1im;
        }
    }
}

/* Hopefully the compiler will optimize out the excessibe assigments to
   temporary variables */
static void
pass4(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 4;
    if (stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble z, z1re, z1im;

            /* Level 0 */
            z = rem[0] - rem[2*gn];
            rem[0] += rem[2*gn];
            rem[2*gn] = z;

            z = imm[0] - imm[2*gn];
            imm[0] += imm[2*gn];
            imm[2*gn] = z;

            z = rem[gn] - rem[3*gn];
            rem[gn] += rem[3*gn];
            rem[3*gn] = z;

            z = imm[gn] - imm[3*gn];
            imm[gn] += imm[3*gn];
            imm[3*gn] = z;

            /* Level 1 */
            z = rem[0] - rem[gn];
            rem[0] += rem[gn];
            rem[gn] = z;

            z = imm[0] - imm[gn];
            imm[0] += imm[gn];
            imm[gn] = z;

            /* Multiplication by i */
            z1re = -imm[3*gn];
            z1im = rem[3*gn];
            rem[3*gn] = rem[2*gn] - z1re;
            imm[3*gn] = imm[2*gn] - z1im;
            rem[2*gn] += z1re;
            imm[2*gn] += z1im;

            /* Fix bit-reversal */
            z = rem[gn];
            rem[gn] = rem[2*gn];
            rem[2*gn] = z;

            z = imm[gn];
            imm[gn] = imm[2*gn];
            imm[2*gn] = z;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble z, z1re, z1im;

            /* Level 0 */
            z = re[stride*m] - re[stride*(2*gn + m)];
            re[stride*m] += re[stride*(2*gn + m)];
            re[stride*(2*gn + m)] = z;

            z = im[stride*m] - im[stride*(2*gn + m)];
            im[stride*m] += im[stride*(2*gn + m)];
            im[stride*(2*gn + m)] = z;

            z = re[stride*(gn + m)] - re[stride*(3*gn + m)];
            re[stride*(gn + m)] += re[stride*(3*gn + m)];
            re[stride*(3*gn + m)] = z;

            z = im[stride*(gn + m)] - im[stride*(3*gn + m)];
            im[stride*(gn + m)] += im[stride*(3*gn + m)];
            im[stride*(3*gn + m)] = z;

            /* Level 1 */
            z = re[stride*m] - re[stride*(gn + m)];
            re[stride*m] += re[stride*(gn + m)];
            re[stride*(gn + m)] = z;

            z = im[stride*m] - im[stride*(gn + m)];
            im[stride*m] += im[stride*(gn + m)];
            im[stride*(gn + m)] = z;

            /* Multiplication by i */
            z1re = -im[stride*(3*gn + m)];
            z1im = re[stride*(3*gn + m)];
            re[stride*(3*gn + m)] = re[stride*(2*gn + m)] - z1re;
            im[stride*(3*gn + m)] = im[stride*(2*gn + m)] - z1im;
            re[stride*(2*gn + m)] += z1re;
            im[stride*(2*gn + m)] += z1im;

            /* Fix bit-reversal */
            z = re[stride*(gn + m)];
            re[stride*(gn + m)] = re[stride*(2*gn + m)];
            re[stride*(2*gn + m)] = z;

            z = im[stride*(gn + m)];
            im[stride*(gn + m)] = im[stride*(2*gn + m)];
            im[stride*(2*gn + m)] = z;
        }
    }
}

/* Hopefully the compiler will optimize out the excessibe assigments to
   temporary variables */
static void
pass5(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 5;
    if (stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble w0re, w0im, w1re, w1im, w2re, w2im, w3re, w3im;
            gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im;

            z0re = rem[gn] + rem[4*gn];
            z0im = imm[gn] + imm[4*gn];
            z1re = rem[gn] - rem[4*gn];
            z1im = imm[gn] - imm[4*gn];
            z2re = rem[2*gn] + rem[3*gn];
            z2im = imm[2*gn] + imm[3*gn];
            z3re = rem[2*gn] - rem[3*gn];
            z3im = imm[2*gn] - imm[3*gn];

            w0re = rem[0] + C5_1*z0re + C5_2*z2re;
            w0im = imm[0] + C5_1*z0im + C5_2*z2im;
            w1re = rem[0] + C5_2*z0re + C5_1*z2re;
            w1im = imm[0] + C5_2*z0im + C5_1*z2im;
            /* Multiplication by i */
            w2re = -S5_1*z1im - S5_2*z3im;
            w2im = S5_1*z1re + S5_2*z3re;
            w3re = -S5_2*z1im + S5_1*z3im;
            w3im = S5_2*z1re - S5_1*z3re;

            rem[0] += z0re + z2re;
            imm[0] += z0im + z2im;
            rem[gn] = w0re + w2re;
            imm[gn] = w0im + w2im;
            rem[2*gn] = w1re + w3re;
            imm[2*gn] = w1im + w3im;
            rem[3*gn] = w1re - w3re;
            imm[3*gn] = w1im - w3im;
            rem[4*gn] = w0re - w2re;
            imm[4*gn] = w0im - w2im;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble w0re, w0im, w1re, w1im, w2re, w2im, w3re, w3im;
            gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im;

            z0re = re[stride*(gn + m)] + re[stride*(4*gn + m)];
            z0im = im[stride*(gn + m)] + im[stride*(4*gn + m)];
            z1re = re[stride*(gn + m)] - re[stride*(4*gn + m)];
            z1im = im[stride*(gn + m)] - im[stride*(4*gn + m)];
            z2re = re[stride*(2*gn + m)] + re[stride*(3*gn + m)];
            z2im = im[stride*(2*gn + m)] + im[stride*(3*gn + m)];
            z3re = re[stride*(2*gn + m)] - re[stride*(3*gn + m)];
            z3im = im[stride*(2*gn + m)] - im[stride*(3*gn + m)];

            w0re = re[stride*m] + C5_1*z0re + C5_2*z2re;
            w0im = im[stride*m] + C5_1*z0im + C5_2*z2im;
            w1re = re[stride*m] + C5_2*z0re + C5_1*z2re;
            w1im = im[stride*m] + C5_2*z0im + C5_1*z2im;
            /* Multiplication by i */
            w2re = -S5_1*z1im - S5_2*z3im;
            w2im = S5_1*z1re + S5_2*z3re;
            w3re = -S5_2*z1im + S5_1*z3im;
            w3im = S5_2*z1re - S5_1*z3re;

            re[stride*m] += z0re + z2re;
            im[stride*m] += z0im + z2im;
            re[stride*(gn + m)] = w0re + w2re;
            im[stride*(gn + m)] = w0im + w2im;
            re[stride*(2*gn + m)] = w1re + w3re;
            im[stride*(2*gn + m)] = w1im + w3im;
            re[stride*(3*gn + m)] = w1re - w3re;
            im[stride*(3*gn + m)] = w1im - w3im;
            re[stride*(4*gn + m)] = w0re - w2re;
            im[stride*(4*gn + m)] = w0im - w2im;
        }
    }
}

static void
pass7(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 7;
    if (stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble w1re, w1im, w2re, w2im, w3re, w3im;
            gdouble w4re, w4im, w5re, w5im, w6re, w6im;
            gdouble z1re, z1im, z2re, z2im, z3re, z3im;
            gdouble z4re, z4im, z5re, z5im, z6re, z6im;

            z1re = rem[gn] + rem[6*gn];
            z1im = imm[gn] + imm[6*gn];
            z6re = rem[gn] - rem[6*gn];
            z6im = imm[gn] - imm[6*gn];
            z2re = rem[2*gn] + rem[5*gn];
            z2im = imm[2*gn] + imm[5*gn];
            z5re = rem[2*gn] - rem[5*gn];
            z5im = imm[2*gn] - imm[5*gn];
            z3re = rem[3*gn] + rem[4*gn];
            z3im = imm[3*gn] + imm[4*gn];
            z4re = rem[3*gn] - rem[4*gn];
            z4im = imm[3*gn] - imm[4*gn];

            w1re = rem[0] + C7_1*z1re + C7_2*z2re + C7_3*z3re;
            w1im = imm[0] + C7_1*z1im + C7_2*z2im + C7_3*z3im;
            w2re = rem[0] + C7_2*z1re + C7_3*z2re + C7_1*z3re;
            w2im = imm[0] + C7_2*z1im + C7_3*z2im + C7_1*z3im;
            w3re = rem[0] + C7_3*z1re + C7_1*z2re + C7_2*z3re;
            w3im = imm[0] + C7_3*z1im + C7_1*z2im + C7_2*z3im;
            /* Multiplication by i */
            w4re = -S7_2*z4im + S7_1*z5im - S7_3*z6im;
            w4im = S7_2*z4re - S7_1*z5re + S7_3*z6re;
            w5re = S7_1*z4im + S7_3*z5im - S7_2*z6im;
            w5im = -S7_1*z4re - S7_3*z5re + S7_2*z6re;
            w6re = -S7_3*z4im - S7_2*z5im - S7_1*z6im;
            w6im = S7_3*z4re + S7_2*z5re + S7_1*z6re;

            rem[0] += z1re + z2re + z3re;
            imm[0] += z1im + z2im + z3im;
            rem[gn] = w1re + w6re;
            imm[gn] = w1im + w6im;
            rem[2*gn] = w2re + w5re;
            imm[2*gn] = w2im + w5im;
            rem[3*gn] = w3re + w4re;
            imm[3*gn] = w3im + w4im;
            rem[4*gn] = w3re - w4re;
            imm[4*gn] = w3im - w4im;
            rem[5*gn] = w2re - w5re;
            imm[5*gn] = w2im - w5im;
            rem[6*gn] = w1re - w6re;
            imm[6*gn] = w1im - w6im;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble w1re, w1im, w2re, w2im, w3re, w3im;
            gdouble w4re, w4im, w5re, w5im, w6re, w6im;
            gdouble z1re, z1im, z2re, z2im, z3re, z3im;
            gdouble z4re, z4im, z5re, z5im, z6re, z6im;

            z1re = re[stride*(gn + m)] + re[stride*(6*gn + m)];
            z1im = im[stride*(gn + m)] + im[stride*(6*gn + m)];
            z6re = re[stride*(gn + m)] - re[stride*(6*gn + m)];
            z6im = im[stride*(gn + m)] - im[stride*(6*gn + m)];
            z2re = re[stride*(2*gn + m)] + re[stride*(5*gn + m)];
            z2im = im[stride*(2*gn + m)] + im[stride*(5*gn + m)];
            z5re = re[stride*(2*gn + m)] - re[stride*(5*gn + m)];
            z5im = im[stride*(2*gn + m)] - im[stride*(5*gn + m)];
            z3re = re[stride*(3*gn + m)] + re[stride*(4*gn + m)];
            z3im = im[stride*(3*gn + m)] + im[stride*(4*gn + m)];
            z4re = re[stride*(3*gn + m)] - re[stride*(4*gn + m)];
            z4im = im[stride*(3*gn + m)] - im[stride*(4*gn + m)];

            w1re = re[stride*m] + C7_1*z1re + C7_2*z2re + C7_3*z3re;
            w1im = im[stride*m] + C7_1*z1im + C7_2*z2im + C7_3*z3im;
            w2re = re[stride*m] + C7_2*z1re + C7_3*z2re + C7_1*z3re;
            w2im = im[stride*m] + C7_2*z1im + C7_3*z2im + C7_1*z3im;
            w3re = re[stride*m] + C7_3*z1re + C7_1*z2re + C7_2*z3re;
            w3im = im[stride*m] + C7_3*z1im + C7_1*z2im + C7_2*z3im;
            /* Multiplication by i */
            w4re = -S7_2*z4im + S7_1*z5im - S7_3*z6im;
            w4im = S7_2*z4re - S7_1*z5re + S7_3*z6re;
            w5re = S7_1*z4im + S7_3*z5im - S7_2*z6im;
            w5im = -S7_1*z4re - S7_3*z5re + S7_2*z6re;
            w6re = -S7_3*z4im - S7_2*z5im - S7_1*z6im;
            w6im = S7_3*z4re + S7_2*z5re + S7_1*z6re;

            re[stride*m] += z1re + z2re + z3re;
            im[stride*m] += z1im + z2im + z3im;
            re[stride*(gn + m)] = w1re + w6re;
            im[stride*(gn + m)] = w1im + w6im;
            re[stride*(2*gn + m)] = w2re + w5re;
            im[stride*(2*gn + m)] = w2im + w5im;
            re[stride*(3*gn + m)] = w3re + w4re;
            im[stride*(3*gn + m)] = w3im + w4im;
            re[stride*(4*gn + m)] = w3re - w4re;
            im[stride*(4*gn + m)] = w3im - w4im;
            re[stride*(5*gn + m)] = w2re - w5re;
            im[stride*(5*gn + m)] = w2im - w5im;
            re[stride*(6*gn + m)] = w1re - w6re;
            im[stride*(6*gn + m)] = w1im - w6im;
        }
    }
}

/* Hopefully the compiler will optimize out the excessibe assigments to
   temporary variables */
static void
pass10(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 10;
    if (FALSE && stride == 1) {
        for (m = 0; m < gn; m++) {
            gdouble *rem = re + m;
            gdouble *imm = im + m;
            gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im, z4re, z4im;
            gdouble z5re, z5im, z6re, z6im, z7re, z7im, z8re, z8im, z9re, z9im;
            gdouble w1re, w1im, w2re, w2im, w3re, w3im, w4re, w4im;
            gdouble w6re, w6im, w7re, w7im, w8re, w8im, w9re, w9im;

            z0re = rem[0] + rem[5*gn];
            z0im = imm[0] + imm[5*gn];
            z5re = rem[0] - rem[5*gn];
            z5im = imm[0] - imm[5*gn];
            z1re = rem[gn] + rem[9*gn];
            z1im = imm[gn] + imm[9*gn];
            z9re = rem[gn] - rem[9*gn];
            z9im = imm[gn] - imm[9*gn];
            z2re = rem[2*gn] + rem[8*gn];
            z2im = imm[2*gn] + imm[8*gn];
            z8re = rem[2*gn] - rem[8*gn];
            z8im = imm[2*gn] - imm[8*gn];
            z3re = rem[3*gn] + rem[7*gn];
            z3im = imm[3*gn] + imm[7*gn];
            z7re = rem[3*gn] - rem[7*gn];
            z7im = imm[3*gn] - imm[7*gn];
            z4re = rem[4*gn] + rem[6*gn];
            z4im = imm[4*gn] + imm[6*gn];
            z6re = rem[4*gn] - rem[6*gn];
            z6im = imm[4*gn] - imm[6*gn];

            w1re = z1re + z4re;
            w1im = z1im + z4im;
            w2re = z2re + z3re;
            w2im = z2im + z3im;
            w3re = z2re - z3re;
            w3im = z2im - z3im;
            w4re = z1re - z4re;
            w4im = z1im - z4im;
            w6re = z6re + z9re;
            w6im = z6im + z9im;
            w7re = z7re + z8re;
            w7im = z7im + z8im;
            w8re = z7re - z8re;
            w8im = z7im - z8im;
            w9re = z6re - z9re;
            w9im = z6im - z9im;

            z1re = z5re + C10_1*w4re + C10_2*w3re;
            z1im = z5im + C10_1*w4im + C10_2*w3im;
            z2re = z0re + C10_2*w1re - C10_1*w2re;
            z2im = z0im + C10_2*w1im - C10_1*w2im;
            z3re = z5re - C10_2*w4re - C10_1*w3re;
            z3im = z5im - C10_2*w4im - C10_1*w3im;
            z4re = z0re - C10_1*w1re + C10_2*w2re;
            z4im = z0im - C10_1*w1im + C10_2*w2im;
            /* Multiplication by i */
            z6re = S10_1*w9im - S10_2*w8im;
            z6im = -S10_1*w9re + S10_2*w8re;
            z7re = -S10_2*w6im + S10_1*w7im;
            z7im = S10_2*w6re - S10_1*w7re;
            z8re = S10_2*w9im + S10_1*w8im;
            z8im = -S10_2*w9re - S10_1*w8re;
            z9re = -S10_1*w6im - S10_2*w7im;
            z9im = S10_1*w6re + S10_2*w7re;

            rem[0] = z0re + w1re + w2re;
            imm[0] = z0im + w1im + w2im;
            rem[gn] = z1re + z9re;
            imm[gn] = z1im + z9im;
            rem[2*gn] = z2re + z8re;
            imm[2*gn] = z2im + z8im;
            rem[3*gn] = z3re + z7re;
            imm[3*gn] = z3im + z7im;
            rem[4*gn] = z4re + z6re;
            imm[4*gn] = z4im + z6im;
            rem[5*gn] = z5re + w3re - w4re;
            imm[5*gn] = z5im + w3im - w4im;
            rem[6*gn] = z4re - z6re;
            imm[6*gn] = z4im - z6im;
            rem[7*gn] = z3re - z7re;
            imm[7*gn] = z3im - z7im;
            rem[8*gn] = z2re - z8re;
            imm[8*gn] = z2im - z8im;
            rem[9*gn] = z1re - z9re;
            imm[9*gn] = z1im - z9im;
        }
    }
    else {
        for (m = 0; m < gn; m++) {
            gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im, z4re, z4im;
            gdouble z5re, z5im, z6re, z6im, z7re, z7im, z8re, z8im, z9re, z9im;
            gdouble w1re, w1im, w2re, w2im, w3re, w3im, w4re, w4im;
            gdouble w6re, w6im, w7re, w7im, w8re, w8im, w9re, w9im;

            z0re = re[stride*m] + re[stride*(5*gn + m)];
            z0im = im[stride*m] + im[stride*(5*gn + m)];
            z5re = re[stride*m] - re[stride*(5*gn + m)];
            z5im = im[stride*m] - im[stride*(5*gn + m)];
            z1re = re[stride*(gn + m)] + re[stride*(9*gn + m)];
            z1im = im[stride*(gn + m)] + im[stride*(9*gn + m)];
            z9re = re[stride*(gn + m)] - re[stride*(9*gn + m)];
            z9im = im[stride*(gn + m)] - im[stride*(9*gn + m)];
            z2re = re[stride*(2*gn + m)] + re[stride*(8*gn + m)];
            z2im = im[stride*(2*gn + m)] + im[stride*(8*gn + m)];
            z8re = re[stride*(2*gn + m)] - re[stride*(8*gn + m)];
            z8im = im[stride*(2*gn + m)] - im[stride*(8*gn + m)];
            z3re = re[stride*(3*gn + m)] + re[stride*(7*gn + m)];
            z3im = im[stride*(3*gn + m)] + im[stride*(7*gn + m)];
            z7re = re[stride*(3*gn + m)] - re[stride*(7*gn + m)];
            z7im = im[stride*(3*gn + m)] - im[stride*(7*gn + m)];
            z4re = re[stride*(4*gn + m)] + re[stride*(6*gn + m)];
            z4im = im[stride*(4*gn + m)] + im[stride*(6*gn + m)];
            z6re = re[stride*(4*gn + m)] - re[stride*(6*gn + m)];
            z6im = im[stride*(4*gn + m)] - im[stride*(6*gn + m)];

            w1re = z1re + z4re;
            w1im = z1im + z4im;
            w2re = z2re + z3re;
            w2im = z2im + z3im;
            w3re = z2re - z3re;
            w3im = z2im - z3im;
            w4re = z1re - z4re;
            w4im = z1im - z4im;
            w6re = z6re + z9re;
            w6im = z6im + z9im;
            w7re = z7re + z8re;
            w7im = z7im + z8im;
            w8re = z7re - z8re;
            w8im = z7im - z8im;
            w9re = z6re - z9re;
            w9im = z6im - z9im;

            z1re = z5re + C10_1*w4re + C10_2*w3re;
            z1im = z5im + C10_1*w4im + C10_2*w3im;
            z2re = z0re + C10_2*w1re - C10_1*w2re;
            z2im = z0im + C10_2*w1im - C10_1*w2im;
            z3re = z5re - C10_2*w4re - C10_1*w3re;
            z3im = z5im - C10_2*w4im - C10_1*w3im;
            z4re = z0re - C10_1*w1re + C10_2*w2re;
            z4im = z0im - C10_1*w1im + C10_2*w2im;
            /* Multiplication by i */
            z6re = S10_1*w9im - S10_2*w8im;
            z6im = -S10_1*w9re + S10_2*w8re;
            z7re = -S10_2*w6im + S10_1*w7im;
            z7im = S10_2*w6re - S10_1*w7re;
            z8re = S10_2*w9im + S10_1*w8im;
            z8im = -S10_2*w9re - S10_1*w8re;
            z9re = -S10_1*w6im - S10_2*w7im;
            z9im = S10_1*w6re + S10_2*w7re;

            re[stride*m] = z0re + w1re + w2re;
            im[stride*m] = z0im + w1im + w2im;
            re[stride*(gn + m)] = z1re + z9re;
            im[stride*(gn + m)] = z1im + z9im;
            re[stride*(2*gn + m)] = z2re + z8re;
            im[stride*(2*gn + m)] = z2im + z8im;
            re[stride*(3*gn + m)] = z3re + z7re;
            im[stride*(3*gn + m)] = z3im + z7im;
            re[stride*(4*gn + m)] = z4re + z6re;
            im[stride*(4*gn + m)] = z4im + z6im;
            re[stride*(5*gn + m)] = z5re + w3re - w4re;
            im[stride*(5*gn + m)] = z5im + w3im - w4im;
            re[stride*(6*gn + m)] = z4re - z6re;
            im[stride*(6*gn + m)] = z4im - z6im;
            re[stride*(7*gn + m)] = z3re - z7re;
            im[stride*(7*gn + m)] = z3im - z7im;
            re[stride*(8*gn + m)] = z2re - z8re;
            im[stride*(8*gn + m)] = z2im - z8im;
            re[stride*(9*gn + m)] = z1re - z9re;
            im[stride*(9*gn + m)] = z1im - z9im;
        }
    }
}

/**
 * gwy_fft_simple:
 * @dir: Transformation direction.
 * @n: Number of data points. Note only certain transform sizes are
 *     implemented.  If gwy_fft_simple() is the current FFT backend, then
 *     gwy_fft_find_nice_size() can provide accepted transform sizes.
 *     If gwy_fft_simple() is not the current FFT backend, you should not
 *     use it.
 * @istride: Input data stride.
 * @in_re: Real part of input data.
 * @in_im: Imaginary part of input data.
 * @ostride: Output data stride.
 * @out_re: Real part of output data.
 * @out_im: Imaginary part of output data.
 *
 * Performs a DFT algorithm.
 *
 * This is a low-level function used by other FFT functions when no better
 * backend is available.
 *
 * Strides are distances between samples in input and output arrays.  Use 1
 * for normal `dense' arrays.  To use gwy_fft_simple() with interleaved arrays,
 * that is with alternating real and imaginary data, call it with
 * @istride=2, @in_re=@complex_array, @in_im=@complex_array+1 (and similarly
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
               const gdouble *in_re,
               const gdouble *in_im,
               gint ostride,
               gdouble *out_re,
               gdouble *out_im)
{
    static GArray *buffer = NULL;

    ButterflyFunc butterfly;
    gdouble *buf_re, *buf_im;
    guint m, p, k, bstride;
    gdouble norm_fact;
    gboolean swapped;

    if (dir != GWY_TRANSFORM_DIRECTION_BACKWARD) {
        GWY_SWAP(const gdouble*, in_re, in_im);
        GWY_SWAP(gdouble*, out_re, out_im);
    }

    swapped = TRUE;
    for (m = 1; m < n; m *= p) {
        k = n/m;
        if (k % 10 == 0)
            p = 10;
        else if (k % 4 == 0)
            p = 4;
        else if (k % 5 == 0)
            p = 5;
        else if (k % 2 == 0)
            p = 2;
        else if (k % 3 == 0)
            p = 3;
        else if (k % 7 == 0)
            p = 7;
        else {
            g_critical("%d (%d) contains unimplemented primes", k, n);
            return;
        }
        swapped = !swapped;
    }

    /* XXX: This is never freed. */
    if (!buffer)
        buffer = g_array_new(FALSE, FALSE, 2*sizeof(gdouble));
    g_array_set_size(buffer, n);
    buf_re = (gdouble*)buffer->data;
    buf_im = buf_re + n;
    bstride = 1;

    if (swapped && n > 1) {
        GWY_SWAP(gdouble*, buf_re, out_re);
        GWY_SWAP(gdouble*, buf_im, out_im);
        GWY_SWAP(guint, bstride, ostride);
    }

    norm_fact = 1.0/sqrt(n);
    for (m = 0; m < n; m++) {
        out_re[ostride*m] = norm_fact*in_re[istride*m];
        out_im[ostride*m] = norm_fact*in_im[istride*m];
    }

    for (m = 1; m < n; m *= p) {
        k = n/m;
        if (k % 10 == 0) {
            p = 10;
            butterfly = pass10;
        }
        else if (k % 4 == 0) {
            p = 4;
            butterfly = pass4;
        }
        else if (k % 5 == 0) {
            p = 5;
            butterfly = pass5;
        }
        else if (k % 2 == 0) {
            p = 2;
            butterfly = pass2;
        }
        else if (k % 3 == 0) {
            p = 3;
            butterfly = pass3;
        }
        else {
            p = 7;
            butterfly = pass7;
        }

        if (m > 1)
            shuffle_and_twiddle(n, m*p, p,
                                bstride, buf_re, buf_im,
                                ostride, out_re, out_im);

        butterfly(n, ostride, out_re, out_im);
        GWY_SWAP(gdouble*, buf_re, out_re);
        GWY_SWAP(gdouble*, buf_im, out_im);
        GWY_SWAP(guint, bstride, ostride);
    }
}

static gdouble
gwy_fft_window_hann(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.5 - 0.5*cos(x);
}

static gdouble
gwy_fft_window_hamming(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.54 - 0.46*cos(x);
}

static gdouble
gwy_fft_window_blackman(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.42 - 0.5*cos(x) + 0.08*cos(2*x);
}

static gdouble
gwy_fft_window_lanczos(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n - G_PI;

    return fabs(x) < 1e-20 ? 1.0 : sin(x)/x;
}

static gdouble
gwy_fft_window_welch(gint i, gint n)
{
    gdouble x = 2.0*i/n - 1.0;

    return 1 - x*x;
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

static gdouble
gwy_fft_window_nuttall(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.355768 - 0.487396*cos(x) + 0.144232*cos(2*x) - 0.012604*cos(3*x);
}

static gdouble
gwy_fft_window_flat_top(gint i, gint n)
{
    gdouble x = 2*G_PI*i/n;

    return (1.0 - 1.93*cos(x) + 1.29*cos(2*x)
            - 0.388*cos(3*x) + 0.032*cos(4*x))/4;
}

static inline gdouble
bessel_I0(gdouble x)
{
    gdouble t, s;
    gint i = 1;

    t = x = x*x/4;
    s = 1.0;
    do {
        s += t;
        i++;
        t *= x/i/i;
    } while (t > 1e-7*s);

    return s + t;
}

/* General function */
static gdouble
gwy_fft_window_kaiser(gint i, gint n, gdouble alpha)
{
    gdouble x = 2.0*i/(n - 1) - 1.0;

    return bessel_I0(G_PI*alpha*sqrt(1.0 - x*x));
}

static gdouble
gwy_fft_window_kaiser25(gint i, gint n)
{
    return gwy_fft_window_kaiser(i, n, 2.5)/373.0206312536293446480;
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
    g_return_if_fail(windowing <= GWY_WINDOWING_KAISER25);
    window = windowings[windowing];
    if (window) {
        for (i = 0; i < n; i++)
            data[i] *= window(i, n);
    }
}

/**
 * gwy_fft_window_data_field:
 * @dfield: A data field.
 * @orientation: Windowing orientation (the same as corresponding FFT
 *               orientation).
 * @windowing: The windowing type to use.
 *
 * Performs windowing of a data field in given direction.
 **/
void
gwy_fft_window_data_field(GwyDataField *dfield,
                          GwyOrientation orientation,
                          GwyWindowingType windowing)
{
    GwyFFTWindowingFunc window;
    gint xres, yres, col, row;
    gdouble *data, *w, q;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(windowing <= GWY_WINDOWING_KAISER25);

    window = windowings[windowing];
    if (!window)
        return;

    xres = dfield->xres;
    yres = dfield->yres;
    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        w = g_new(gdouble, xres);
        for (col = 0; col < xres; col++)
            w[col] = window(col, xres);
        for (row = 0; row < yres; row++) {
            data = dfield->data + row*xres;
            for (col = 0; col < xres; col++)
                data[col] *= w[col];
        }
        g_free(w);
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
 * The simple one-dimensional FFT algorithm gwy_fft_simple() is used as
 * a fallback by other functions when better implementation (FFTW3) is not
 * available.
 *
 * It works only on data sizes that are powers of 2.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
