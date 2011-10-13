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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * How this all works.
 *
 * There are four main components:
 *
 * - shuffle_and_twiddle() reorganizes data from one buffer to another,
 *   to the order expected by the pass functions below (which is always
 *   maximized stride n/p, unfortunately) and applied twiddle factors
 *
 * - pass2(), ..., pass10() perform subtransforms with sizes p=2-10 on
 *   arrays of size n, i.e. they perform n/p identical transforms with
 *   stride n/p, these are hand-coded
 *
 * - gpass() performs a general small-prime subtransform of size p, it is
 *   O(p^2), however it makes use of some general block symmetries and thus
 *   it outperforms bluestein() on smaller prime sizes
 *
 * - bluestein() is a Bluestein's arbitrary-size O(n log(n)) transform
 *   algorithm that re-expresses it via a cyclic convolution of larger
 *   but more factorable size, it recursively calls gwy_fft_simple() and it
 *   is currently only used in on big step for everything that remains
 *   after chopping off factors that passP() and gpass() can handle
 *
 * The driver routine gwy_fft_simple() decides whether a bluestein() pass is
 * necessary, performs it as the first step if it is, and then continues
 * with interleaved passP()/gpass() and shuffle_and_twiddle() for the
 * individual factors, using a temporary storage buffer with stride 1 (that
 * is never freed).
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/simplefft.h>

#define C3_1 0.5
#define S3_1 0.86602540378443864676372317075293618347140262690518

#define C10_1 0.80901699437494742410229341718281905886015458990288
#define C10_2 0.30901699437494742410229341718281905886015458990289
#define S10_1 0.58778525229247312916870595463907276859765243764313
#define S10_2 0.95105651629515357211643933337938214340569863412574

#define C5_1 C10_2
#define C5_2 -C10_1
#define S5_1 S10_2
#define S5_2 S10_1

#define C7_1 0.62348980185873353052500488400423981063227473089641
#define C7_2 -0.22252093395631440428890256449679475946635556876452
#define C7_3 -0.90096886790241912623610231950744505116591916213184
#define S7_1 0.78183148246802980870844452667405775023233451870867
#define S7_2 0.97492791218182360701813168299393121723278580062000
#define S7_3 0.43388373911755812047576833284835875460999072778748

#define S8_1 .70710678118654752440084436210484903928483593768846

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

static guint smooth_upper_bound(guint n);

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

/**
 * gwy_fft_find_nice_size:
 * @size: Transform size.
 *
 * Finds a nice-for-FFT array size.
 *
 * The `nice' means three properties are guaranteed:
 * it is greater than or equal to @size;
 * it can be directly used with current FFT backend without scaling
 * (since 2.8 this is true for any size);
 * and the transform is fast, i.e. the number is highly factorable.
 *
 * To be compatible with Gwyddion <= 2.7 one has to pass only data fields and
 * lines with sizes returned by this function to raw integral transforms.
 * Otherwise this function is mainly useful if you extend and pad the input
 * data for other reasons and thus have the freedom to choose a convenient
 * transform size.
 *
 * Returns: A nice FFT array size.
 **/
gint
gwy_fft_find_nice_size(gint size)
{
    if (size <= 16)
        return size;

    return smooth_upper_bound(size);
}

/***** Scratch buffers {{{ **********************************************/
/* We don't like GArray because it ensures the data will stay there when the
 * array size changes, which is a waste of time if it includes copying.
 * Also, we'd like to have plain gdouble* arrays, the administrative stuff
 * should not be visible. */
#define _GWY_SCRATCH_BUFFER_ALIGNMENT 16
#define _GWY_SCRATCH_BUFFER_BLOCK 16

#define _GWY_SCRATCH_BUFFER_GET(b) \
    ((_GwyScratchBuffer*)(((guchar*)(b))-_GWY_SCRATCH_BUFFER_ALIGNMENT))

#define _GWY_SCRATCH_BUFFER_ALIGN(n, l) ((MAX((n), 1) + (l)-1)/(l)*(l))

#if (GLIB_MAJOR_VERSION > 2 \
     || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 10))
#define _gwy_scratch_buffer_free_backend(b, l) \
    g_slice_free1((l)*sizeof(gdouble), (b))
#define _gwy_scratch_buffer_alloc_backend(l) \
    g_slice_alloc((l)*sizeof(gdouble) + sizeof(_GwyScratchBufferHead))
#else
#define _gwy_scratch_buffer_free_backend(b, l) \
    g_free(b)
#define _gwy_scratch_buffer_alloc_backend(b, l) \
    g_malloc((l)*sizeof(gdouble) + sizeof(_GwyScratchBufferHead))
#endif

typedef union {
    struct {
        gsize alloc_len;
    } info;
    guchar keep_memory_nicely_aligned[_GWY_SCRATCH_BUFFER_ALIGNMENT];
} _GwyScratchBufferHead;

typedef struct {
    _GwyScratchBufferHead head;
    gdouble data[1];  /* Don't tempt compilers even more. */
} _GwyScratchBuffer;

static inline void
_gwy_scratch_buffer_free(gdouble *buffer)
{
    _GwyScratchBuffer *buf;

    if (buffer) {
        buf = _GWY_SCRATCH_BUFFER_GET(buffer);
        _gwy_scratch_buffer_free_backend(buf, buf->head.info.alloc_len);
    }
}

static inline gdouble*
_gwy_scratch_buffer_ensure(gdouble *buffer, guint n)
{
    _GwyScratchBuffer *buf;

    if (buffer) {
        buf = _GWY_SCRATCH_BUFFER_GET(buffer);
        if (n <= buf->head.info.alloc_len)
            return buffer;

        _gwy_scratch_buffer_free_backend(buf, buf->head.info.alloc_len);
    }

    n = _GWY_SCRATCH_BUFFER_ALIGN(n, _GWY_SCRATCH_BUFFER_ALIGNMENT);
    buf = _gwy_scratch_buffer_alloc_backend(n);
    buf->head.info.alloc_len = n;
    return &buf->data[0];
}
/***** }}} **************************************************************/


/**
 * shuffle_and_twiddle:
 * @gn: The total array size.
 * @gm: The next already transformed size, i.e. the product of all factors
 *      we have already handled, including @p.
 * @p: The factor we want to handle now.
 * @istride: Input data stride.
 * @in_re: Real part of input data.
 * @in_im: Imaginary part of input data.
 * @ostride: Output data stride.
 * @out_re: Real part of output data.
 * @out_im: Imaginary part of output data.
 *
 * Move data between two buffers, ensuring the result will have stride @gn/@p
 * and applying twiddle factors.
 **/
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

/* Butterflies. {{{
 * Hopefully the compiler will optimize out the excessibe assigments to
 * temporary variables. */
static void
pass2(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 2;
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

static void
pass3(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 3;
    for (m = 0; m < gn; m++) {
        gdouble z1re, z1im, z2re, z2im;

        z1re = re[stride*(gn + m)] + re[stride*(2*gn + m)];
        z1im = im[stride*(gn + m)] + im[stride*(2*gn + m)];
        /* Multiplication by i */
        z2re = (im[stride*(2*gn + m)] - im[stride*(gn + m)])*S3_1;
        z2im = (re[stride*(gn + m)] - re[stride*(2*gn + m)])*S3_1;
        re[stride*(2*gn + m)] = re[stride*m] - (z2re + 0.5*z1re);
        im[stride*(2*gn + m)] = im[stride*m] - (z2im + 0.5*z1im);
        re[stride*(gn + m)] = re[stride*m] + (z2re - 0.5*z1re);
        im[stride*(gn + m)] = im[stride*m] + (z2im - 0.5*z1im);
        re[stride*m] += z1re;
        im[stride*m] += z1im;
    }
}

static void
pass4(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 4;
    for (m = 0; m < gn; m++) {
        gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im;

        /* Level 0 */
        z0re = re[stride*m] + re[stride*(2*gn + m)];
        z0im = im[stride*m] + im[stride*(2*gn + m)];
        z1re = re[stride*(gn + m)] + re[stride*(3*gn + m)];
        z1im = im[stride*(gn + m)] + im[stride*(3*gn + m)];
        z2re = re[stride*m] - re[stride*(2*gn + m)];
        z2im = im[stride*m] - im[stride*(2*gn + m)];
        z3re = re[stride*(gn + m)] - re[stride*(3*gn + m)];
        z3im = im[stride*(gn + m)] - im[stride*(3*gn + m)];

        /* Level 1 */
        re[stride*m] = z0re + z1re;
        im[stride*m] = z0im + z1im;
        /* Multiplication by i */
        re[stride*(gn + m)] = z2re - z3im;
        im[stride*(gn + m)] = z2im + z3re;
        re[stride*(2*gn + m)] = z0re - z1re;
        im[stride*(2*gn + m)] = z0im - z1im;
        /* Multiplication by i */
        re[stride*(3*gn + m)] = z2re + z3im;
        im[stride*(3*gn + m)] = z2im - z3re;
    }
}

static void
pass5(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 5;
    for (m = 0; m < gn; m++) {
        gdouble w0re, w0im, w1re, w1im, w2re, w2im, w3re, w3im;
        gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im;

        /* Level 0 */
        z0re = re[stride*(gn + m)] + re[stride*(4*gn + m)];
        z0im = im[stride*(gn + m)] + im[stride*(4*gn + m)];
        z1re = re[stride*(gn + m)] - re[stride*(4*gn + m)];
        z1im = im[stride*(gn + m)] - im[stride*(4*gn + m)];
        z2re = re[stride*(2*gn + m)] + re[stride*(3*gn + m)];
        z2im = im[stride*(2*gn + m)] + im[stride*(3*gn + m)];
        z3re = re[stride*(2*gn + m)] - re[stride*(3*gn + m)];
        z3im = im[stride*(2*gn + m)] - im[stride*(3*gn + m)];

        /* Level 1 */
        w0re = re[stride*m] + C5_1*z0re + C5_2*z2re;
        w0im = im[stride*m] + C5_1*z0im + C5_2*z2im;
        w1re = re[stride*m] + C5_2*z0re + C5_1*z2re;
        w1im = im[stride*m] + C5_2*z0im + C5_1*z2im;
        /* Multiplication by i */
        w2re = -S5_1*z1im - S5_2*z3im;
        w2im = S5_1*z1re + S5_2*z3re;
        w3re = -S5_2*z1im + S5_1*z3im;
        w3im = S5_2*z1re - S5_1*z3re;

        /* Level 2 */
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

static void
pass6(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 6;
    for (m = 0; m < gn; m++) {
        gdouble w1re, w1im, w2re, w2im;
        gdouble w4re, w4im, w5re, w5im;
        gdouble z0re, z0im, z1re, z1im, z2re, z2im;
        gdouble z3re, z3im, z4re, z4im, z5re, z5im;

        /* Level 0 */
        z0re = re[stride*m] + re[stride*(3*gn + m)];
        z0im = im[stride*m] + im[stride*(3*gn + m)];
        z3re = re[stride*m] - re[stride*(3*gn + m)];
        z3im = im[stride*m] - im[stride*(3*gn + m)];
        z1re = re[stride*(gn + m)] + re[stride*(5*gn + m)];
        z1im = im[stride*(gn + m)] + im[stride*(5*gn + m)];
        z5re = re[stride*(gn + m)] - re[stride*(5*gn + m)];
        z5im = im[stride*(gn + m)] - im[stride*(5*gn + m)];
        z2re = re[stride*(2*gn + m)] + re[stride*(4*gn + m)];
        z2im = im[stride*(2*gn + m)] + im[stride*(4*gn + m)];
        z4re = re[stride*(2*gn + m)] - re[stride*(4*gn + m)];
        z4im = im[stride*(2*gn + m)] - im[stride*(4*gn + m)];

        /* Level 1 and Level 2 for indices 0 and 3 */
        w1re = z1re + z2re;
        w1im = z1im + z2im;
        w2re = z1re - z2re;
        w2im = z1im - z2im;
        re[stride*m] = z0re + w1re;
        im[stride*m] = z0im + w1im;
        re[stride*(3*gn + m)] = z3re - w2re;
        im[stride*(3*gn + m)] = z3im - w2im;
        w1re *= C3_1;
        w1im *= C3_1;
        w2re *= C3_1;
        w2im *= C3_1;
        /* Multiplication by i */
        w4re = -S3_1*(z4im + z5im);
        w4im = S3_1*(z4re + z5re);
        w5re = -S3_1*(z4im - z5im);
        w5im = S3_1*(z4re - z5re);

        /* Level 2 */
        re[stride*(gn + m)] = z3re + w2re + w4re;
        im[stride*(gn + m)] = z3im + w2im + w4im;
        re[stride*(2*gn + m)] = z0re - w1re - w5re;
        im[stride*(2*gn + m)] = z0im - w1im - w5im;
        re[stride*(4*gn + m)] = z0re - w1re + w5re;
        im[stride*(4*gn + m)] = z0im - w1im + w5im;
        re[stride*(5*gn + m)] = z3re + w2re - w4re;
        im[stride*(5*gn + m)] = z3im + w2im - w4im;
    }
}

static void
pass7(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 7;
    for (m = 0; m < gn; m++) {
        gdouble w1re, w1im, w2re, w2im, w3re, w3im;
        gdouble w4re, w4im, w5re, w5im, w6re, w6im;
        gdouble z1re, z1im, z2re, z2im, z3re, z3im;
        gdouble z4re, z4im, z5re, z5im, z6re, z6im;

        /* Level 0 */
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

        /* Level 2 */
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

        /* Level 3 */
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

static void
pass8(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 8;
    for (m = 0; m < gn; m++) {
        gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im;
        gdouble z4re, z4im, z5re, z5im, z6re, z6im, z7re, z7im;
        gdouble w0re, w0im, w1re, w1im, w2re, w2im, w3re, w3im;
        gdouble w4re, w4im, w5re, w5im, w6re, w6im, w7re, w7im;

        /* Level 0 */
        z0re = re[stride*m] + re[stride*(4*gn + m)];
        z0im = im[stride*m] + im[stride*(4*gn + m)];
        z1re = re[stride*(gn + m)] + re[stride*(5*gn + m)];
        z1im = im[stride*(gn + m)] + im[stride*(5*gn + m)];
        z2re = re[stride*(2*gn + m)] + re[stride*(6*gn + m)];
        z2im = im[stride*(2*gn + m)] + im[stride*(6*gn + m)];
        z3re = re[stride*(3*gn + m)] + re[stride*(7*gn + m)];
        z3im = im[stride*(3*gn + m)] + im[stride*(7*gn + m)];
        z4re = re[stride*m] - re[stride*(4*gn + m)];
        z4im = im[stride*m] - im[stride*(4*gn + m)];
        z5re = re[stride*(gn + m)] - re[stride*(5*gn + m)];
        z5im = im[stride*(gn + m)] - im[stride*(5*gn + m)];
        z6re = re[stride*(2*gn + m)] - re[stride*(6*gn + m)];
        z6im = im[stride*(2*gn + m)] - im[stride*(6*gn + m)];
        z7re = re[stride*(3*gn + m)] - re[stride*(7*gn + m)];
        z7im = im[stride*(3*gn + m)] - im[stride*(7*gn + m)];

        /* Level 1 */
        w0re = z0re + z2re;
        w0im = z0im + z2im;
        w1re = z1re + z3re;
        w1im = z1im + z3im;
        w2re = z0re - z2re;
        w2im = z0im - z2im;
        w3re = z1re - z3re;
        w3im = z1im - z3im;
        /* Multiplication by i */
        w4re = z4re - z6im;
        w4im = z4im + z6re;
        /* Multiplication by i */
        w5re = -(z5im + z7im);
        w5im = z5re + z7re;
        /* Multiplication by i */
        w6re = z4re + z6im;
        w6im = z4im - z6re;
        w7re = z5re - z7re;
        w7im = z5im - z7im;

        /* Level 2 and 3 */
        z5re = S8_1*(w5re + w7re);
        z5im = S8_1*(w5im + w7im);
        z7re = S8_1*(w5re - w7re);
        z7im = S8_1*(w5im - w7im);
        re[stride*m] = w0re + w1re;
        im[stride*m] = w0im + w1im;
        re[stride*(gn + m)] = w4re + z5re;
        im[stride*(gn + m)] = w4im + z5im;
        /* Multiplication by i */
        re[stride*(2*gn + m)] = w2re - w3im;
        im[stride*(2*gn + m)] = w2im + w3re;
        re[stride*(3*gn + m)] = w6re + z7re;
        im[stride*(3*gn + m)] = w6im + z7im;
        re[stride*(4*gn + m)] = w0re - w1re;
        im[stride*(4*gn + m)] = w0im - w1im;
        re[stride*(5*gn + m)] = w4re - z5re;
        im[stride*(5*gn + m)] = w4im - z5im;
        /* Multiplication by i */
        re[stride*(6*gn + m)] = w2re + w3im;
        im[stride*(6*gn + m)] = w2im - w3re;
        re[stride*(7*gn + m)] = w6re - z7re;
        im[stride*(7*gn + m)] = w6im - z7im;
    }
}

static void
pass10(guint gn, guint stride, gdouble *re, gdouble *im)
{
    guint m;

    gn /= 10;
    for (m = 0; m < gn; m++) {
        gdouble z0re, z0im, z1re, z1im, z2re, z2im, z3re, z3im, z4re, z4im;
        gdouble z5re, z5im, z6re, z6im, z7re, z7im, z8re, z8im, z9re, z9im;
        gdouble w1re, w1im, w2re, w2im, w3re, w3im, w4re, w4im;
        gdouble w6re, w6im, w7re, w7im, w8re, w8im, w9re, w9im;

        /* Level 0 */
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

        /* Level 1 */
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

        /* Level 2 (recycling z, except for z0 and z5) */
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

        /* Level 3 */
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
/* }}} */

/**
 * smooth_upper_bound:
 * @n: A number.
 *
 * Finds a smooth (highly factorable) number larger or equal to @n.
 *
 * Returns: A smooth number larger or equal to @n.
 **/
static guint
smooth_upper_bound(guint n)
{
    static const guint primes[] = { 2, 3, 5, 7 };

    guint j, p, r;

    for (r = 1; ; ) {
        /* the factorable part */
        for (j = 0; j < G_N_ELEMENTS(primes); j++) {
            p = primes[j];
            while (n % p == 0) {
                n /= p;
                r *= p;
            }
        }

        if (n == 1)
            return r;

        /* gosh... make it factorable again */
        n++;
    }
}

/**
 * bluestein:
 * @n: The transform size and the array size (unlike passP() and gpass(),
 *     this function performs only a single transform).
 * @istride: Input data stride.
 * @in_re: Real part of input data.
 * @in_im: Imaginary part of input data.
 * @ostride: Output data stride.
 * @out_re: Real part of output data.
 * @out_im: Imaginary part of output data.
 *
 * Performs Bluestein's arbitrary-size FFT.
 *
 * Note it calls gwy_fft_simple().
 **/
static void
bluestein(guint n,
          guint istride, const gdouble *in_re, const gdouble *in_im,
          guint ostride, gdouble *out_re, gdouble *out_im)
{
    static gdouble *bre = NULL;
    static guint bn = 0;
    static gdouble *are = NULL;

    gdouble *aim, *bim;
    gdouble q;
    guint j, nfft;

    nfft = smooth_upper_bound(2*n - 1);

    /* Calculate chirp b, Fb */
    if (bn != n) {
        bre = _gwy_scratch_buffer_ensure(bre, 4*nfft);
        bn = n;

        bim = bre + nfft;
        bre[0] = 1.0;
        bim[0] = 0.0;
        for (j = 1; j < (n + 1)/2; j++) {
            _gwy_sincos(G_PI*j*j/n, bim + j, bre + j);
            bre[nfft - j]   = bre[j];
            bim[nfft - j]   = bim[j];
            bre[nfft-n + j] = bre[n - j] = -bre[j];
            bim[nfft-n + j] = bim[n - j] = -bim[j];
        }

        for (j = n; j <= nfft-n; j++)
            bre[j] = bim[j] = 0.0;

        gwy_fft_simple(GWY_TRANSFORM_DIRECTION_FORWARD, nfft,
                       1, bre, bim, 1, bre + 2*nfft, bim + 2*nfft);
    }
    else
        bim = bre + nfft;

    /* Build zero-extended premultiplied a, Fa */
    are = _gwy_scratch_buffer_ensure(are, 4*nfft);
    aim = are + nfft;

    for (j = 0; j < n; j++) {
        are[j] = bre[j]*in_re[istride*j] + bim[j]*in_im[istride*j];
        aim[j] = bre[j]*in_im[istride*j] - bim[j]*in_re[istride*j];
    }
    gwy_clear(are + n, nfft - n);
    gwy_clear(aim + n, nfft - n);
    gwy_fft_simple(GWY_TRANSFORM_DIRECTION_FORWARD, nfft,
                   1, are, aim, 1, are + 2*nfft, aim + 2*nfft);

    /* Multiply Fa*Fb into Fa and transform back */
    are += 2*nfft;
    aim += 2*nfft;
    bre += 2*nfft;
    bim += 2*nfft;
    for (j = 0; j < nfft; j++) {
        gdouble x = are[j];

        are[j] = bre[j]*are[j] - bim[j]*aim[j];
        aim[j] = bre[j]*aim[j] + bim[j]*x;
    }
    are -= 2*nfft;
    aim -= 2*nfft;
    bre -= 2*nfft;
    bim -= 2*nfft;
    gwy_fft_simple(GWY_TRANSFORM_DIRECTION_BACKWARD, nfft,
                   1, are + 2*nfft, aim + 2*nfft, 1, are, aim);

    /* Store result into out */
    q = sqrt((gdouble)nfft/n);
    for (j = 0; j < n; j++) {
        out_re[ostride*j] = q*(are[j]*bre[j] + aim[j]*bim[j]);
        out_im[ostride*j] = q*(aim[j]*bre[j] - are[j]*bim[j]);
    }
}

/**
 * gpass:
 * @p: The factor we want to handle now.
 * @gn: The total array size.
 * @stride: Array stride.
 * @re: Real data.
 * @im: Imaginary data.
 *
 * Performs a general prime-sized butterfly pass.
 *
 * This is a rather naive O(N^2) method.
 *
 * All memory is stack-allocated, do not pass large @p values!
 **/
static void
gpass(guint p, guint gn,
      guint stride, gdouble *re, gdouble *im)
{
    gdouble *fc, *fs, *wer, *wor, *wei, *woi;
    gdouble ucr, uci, usr, usi;
    gint *idx;
    guint q, m, n, j;
    gint k;

    gn /= p;
    q = (p - 1)/2;
    fc = g_newa(gdouble, 2*q);
    fs = fc + q;
    for (j = 0; j < q; j++)
        _gwy_sincos(2*G_PI*(j + 1)/p, fs + j, fc + j);

    idx = g_newa(gint, q*q);
    for (n = 0; n < q; n++) {
        for (j = 0; j < q; j++) {
            k = (n + 1)*(j + 1) % p;
            idx[n*q + j] = (k > (gint)q) ? k-(gint)p : k;
        }
    }

    wer = g_newa(gdouble, 4*q);
    wei = wer + q;
    wor = wer + 2*q;
    woi = wor + q;
    for (m = 0; m < gn; m++) {
        /* even/odd blocks */
        for (j = 1; j <= q; j++) {
            wer[j-1] = re[stride*(j*gn + m)] + re[stride*((p - j)*gn + m)];
            wor[j-1] = re[stride*(j*gn + m)] - re[stride*((p - j)*gn + m)];
            wei[j-1] = im[stride*(j*gn + m)] + im[stride*((p - j)*gn + m)];
            woi[j-1] = im[stride*(j*gn + m)] - im[stride*((p - j)*gn + m)];
        }
        /* block sums */
        for (n = 0; n < q; n++) {
            ucr = re[stride*m];
            uci = im[stride*m];
            usr = 0.0;
            usi = 0.0;
            for (j = 0; j < q; j++) {
                if ((k = idx[n*q + j]) > 0) {
                    ucr += fc[k-1]*wer[j];
                    uci += fc[k-1]*wei[j];
                    usr += fs[k-1]*wor[j];
                    usi += fs[k-1]*woi[j];
                }
                else {
                    ucr += fc[-k-1]*wer[j];
                    uci += fc[-k-1]*wei[j];
                    usr -= fs[-k-1]*wor[j];
                    usi -= fs[-k-1]*woi[j];
                }
            }
            re[stride*((n + 1)*gn + m)] = ucr - usi;
            im[stride*((n + 1)*gn + m)] = uci + usr;
            re[stride*((p-1 - n)*gn + m)] = ucr + usi;
            im[stride*((p-1 - n)*gn + m)] = uci - usr;
        }
        /* first row */
        for (j = 0; j < q; j++) {
            re[stride*m] += wer[j];
            im[stride*m] += wei[j];
        }
    }
}

/**
 * analyse_size:
 * @n: A number (transform size).
 * @pp: An array to put its factors to.  The caller is responsible for it
 *      being large enough.
 *
 * Factors a transform size into suitable subtransform sizes.
 *
 * The factors are not necessarily primes, they are either factors we have
 * passP() routine for (and larger compound factors are preferred then), or
 * they are prime factors handled by gpass().  If anything remains, it is
 * put to the last item and the return value is negated.
 *
 * Returns: The number of factors put into @pp, negated if the last factor
 *          is a Big Ugly Number(TM).
 **/
static gint
analyse_size(guint n, guint *pp)
{
    guint m, p, k;
    gint np;

    for (m = 1, np = 0; m < n; m *= p, np++) {
        k = n/m;
        if (k % 5 == 0)
            p = (k % 2 == 0) ? 10 : 5;
        else if (k % 3 == 0)
            p = (k % 2 == 0) ? 6 : 3;
        else if (k % 2 == 0)
            p = (k % 4 == 0) ? ((k % 8 == 0 && k != 16) ? 8 : 4) : 2;
        else if (k % 7 == 0)
            p = 7;
        /* Out of luck with nice factors.  Try the less nice ones. */
        else {
            /* gpass() ceases to be competitive with bluestein() around 70.
             * Also, the required stack space exceeds 1kB. */
            static const guint primes[] = {
                11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67
            };

            for (m = 0; m < G_N_ELEMENTS(primes); m++) {
                p = primes[m];
                while (k % p == 0) {
                    pp[np] = p;
                    np++;
                    k /= p;
                }
                if (k == 1)
                    return np;
            }
            /* Ugly large prime factors still remain.
             * Indicate them by a negative return value. */
            pp[np] = k;
            np++;

            return -np;
        }
        pp[np] = p;
    }

    return np;
}

static void
normalize(guint n, gdouble norm_fact,
          guint istride, const gdouble *in_re, const gdouble *in_im,
          guint ostride, gdouble *out_re, gdouble *out_im)
{
    guint j;

    for (j = 0; j < n; j++) {
        out_re[ostride*j] = norm_fact*in_re[istride*j];
        out_im[ostride*j] = norm_fact*in_im[istride*j];
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
    static const ButterflyFunc butterflies[] = {
        NULL, NULL, pass2, pass3, pass4,
        pass5, pass6, pass7, pass8, NULL,
        pass10
    };

    static gdouble *buffer = NULL;

    guint *pp;
    gdouble *buf_re, *buf_im;
    guint m, j, p, k, bstride;
    gint np;
    gdouble norm_fact;

    /* The subroutines are written for +i in the exponent.  Shoot me. */
    if (dir != GWY_TRANSFORM_DIRECTION_BACKWARD) {
        GWY_SWAP(const gdouble*, in_re, in_im);
        GWY_SWAP(gdouble*, out_re, out_im);
    }

    m = 1;
    pp = g_newa(guint, 21);
    np = analyse_size(n, pp);
#ifdef DEBUG
    {
        GString *str;

        str = g_string_new(NULL);
        g_string_append_printf(str, "%u:", n);
        for (j = 0; j < (guint)ABS(np); j++)
            g_string_append_printf(str, " %u", pp[j]);
        if (np < 0)
            g_string_append(str, "(!)");
        gwy_debug("factorization: %s", str->str);
        g_string_free(str, TRUE);
    }
#endif
    /* This is exteremely simplistic.  Although we do not have codelets for
     * the factors, we could still factorize it and use much smaller transform
     * sizes in bluestein(). */
    if (np < 0) {
        /* bluestein() calls recursively gwy_fft_simple() and the contents of
         * buf[] is overwriten. */
        np = -np-1;
        m = pp[np];
        k = n/m;
        /* FIXME: This has a terrible memory access pattern.
         * The swapped arrays are all right, bluestein() uses a normal
         * sign convention, unlike everything else here. */
        for (j = 0; j < k; j++)
            bluestein(m,
                      k*istride, in_im + j*istride, in_re + j*istride,
                      k*ostride, out_im + j*ostride, out_re + j*ostride);
        if (k == 1)
            return;
    }

    /* XXX: This is never freed. */
    buf_re = buffer = _gwy_scratch_buffer_ensure(buffer, 2*n);
    buf_im = buf_re + n;
    bstride = 1;

    /* Here it gets hairy.
     * We can have the data either in out or in in.
     * And we may want them either in out or in buf. */
    norm_fact = sqrt((gdouble)m/n);
    if (m > 1) {
        if (np % 2 == 0) {
            normalize(n, norm_fact,
                      ostride, out_re, out_im, ostride, out_re, out_im);

            GWY_SWAP(gdouble*, buf_re, out_re);
            GWY_SWAP(gdouble*, buf_im, out_im);
            GWY_SWAP(guint, bstride, ostride);
        }
        else {
            normalize(n, norm_fact,
                      ostride, out_re, out_im, bstride, buf_re, buf_im);
        }
    }
    else {
        if (np % 2 == 0 && n > 1) {
            GWY_SWAP(gdouble*, buf_re, out_re);
            GWY_SWAP(gdouble*, buf_im, out_im);
            GWY_SWAP(guint, bstride, ostride);
        }
        normalize(n, norm_fact,
                  istride, in_re, in_im, ostride, out_re, out_im);
    }

    /* Cooley-Tukey */
    for (k = 0; k < (guint)np; k++, m *= p) {
        p = pp[k];
        if (m > 1)
            shuffle_and_twiddle(n, m*p, p,
                                bstride, buf_re, buf_im,
                                ostride, out_re, out_im);

        if (p < G_N_ELEMENTS(butterflies))
            butterflies[p](n, ostride, out_re, out_im);
        else
            gpass(p, n, ostride, out_re, out_im);

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
    x = 1.0 - x*x;
    x = MAX(x, 0.0);

    return bessel_I0(G_PI*alpha*sqrt(x));
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
 * @title: simpleFFT
 * @short_description: Simple FFT algorithm
 * @see_also: <link linkend="libgwyprocess-inttrans">inttrans</link>
 *            -- high-level integral transform functions
 *
 * The simple one-dimensional FFT algorithm gwy_fft_simple() is used as
 * a fallback by other functions when a better implementation (FFTW3) is not
 * available.
 *
 * You should not use it directly, as it is a waste of resources
 * if FFTW3 backed is in use, neither you should feel any need to, as
 * high-level functions such as gwy_data_field_2dfft() are available.
 *
 * Up to version 2.7 simpleFFT works only with certain tranform sizes, mostly
 * powers of 2.  Since 2.8 it can handle arbitrary tranform sizes, although
 * sizes with large prime factors can be quite slow (still O(n*log(n)) though).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
