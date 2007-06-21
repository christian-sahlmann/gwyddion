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

/* Good FFT array sizes for extended and resampled arrays.
 *
 * Since extending and resampling always involves an O(N) part -- N being the
 * extended array size -- that may even be dominant, it isn't wise to use the
 * fastest possible FFT if it requires considerably larger array.  Following
 * numbers represent a reasonable compromise tested on a few platforms */
static const guint nice_fft_num[] = {
#ifdef HAVE_FFTW3
    /* {{{ Nice transform sizes for FFTW3 */
         16,     18,     20,     21,     22,     24,     25,     27,     28,
         32,     33,     35,     36,     40,     42,     44,     45,     48,
         49,     50,     54,     55,     56,     60,     64,     66,     70,
         72,     75,     77,     80,     81,     84,     90,     96,     98,
         99,    100,    105,    108,    110,    112,    120,    126,    128,
        132,    135,    140,    144,    150,    160,    165,    168,    176,
        180,    192,    196,    198,    200,    216,    224,    225,    240,
        256,    264,    280,    288,    294,    300,    320,    324,    330,
        336,    343,    350,    352,    360,    384,    385,    392,    396,
        400,    405,    420,    432,    448,    450,    480,    500,    512,
        525,    528,    540,    550,    576,    588,    594,    600,    640,
        648,    660,    672,    675,    686,    700,    704,    720,    735,
        768,    770,    784,    792,    800,    810,    825,    840,    864,
        880,    896,    900,    924,    960,    972,    980,   1008,   1024,
       1029,   1056,   1080,   1100,   1120,   1125,   1134,   1155,   1176,
       1188,   1200,   1215,   1232,   1260,   1280,   1296,   1344,   1350,
       1372,   1400,   1408,   1440,   1470,   1500,   1512,   1536,   1540,
       1568,   1575,   1584,   1600,   1620,   1650,   1680,   1728,   1760,
       1764,   1792,   1800,   1848,   1890,   1920,   1944,   1960,   2000,
       2016,   2048,   2058,   2100,   2112,   2160,   2200,   2205,   2240,
       2250,   2304,   2310,   2352,   2400,   2430,   2450,   2464,   2475,
       2520,   2560,   2592,   2640,   2688,   2700,   2744,   2772,   2800,
       2816,   2880,   2916,   2940,   3000,   3024,   3072,   3136,   3150,
       3168,   3200,   3240,   3300,   3360,   3375,   3456,   3500,   3520,
       3528,   3584,   3600,   3645,   3675,   3696,   3750,   3780,   3840,
       3888,   3920,   4032,   4096,   4116,   4125,   4158,   4200,   4224,
       4320,   4374,   4400,   4410,   4480,   4500,   4608,   4620,   4704,
       4725,   4752,   4800,   4802,   4860,   4900,   4928,   5000,   5040,
       5120,   5184,   5250,   5280,   5376,   5400,   5488,   5500,   5544,
       5600,   5625,   5632,   5670,   5760,   5832,   5880,   6000,   6048,
       6144,   6160,   6272,   6300,   6336,   6400,   6480,   6600,   6615,
       6720,   6750,   6912,   7000,   7040,   7056,   7168,   7200,   7203,
       7290,   7350,   7392,   7425,   7500,   7546,   7700,   7875,   7938,
       8000,   8019,   8085,   8192,   8232,   8250,   8316,   8505,   8575,
       8624,   8748,   8750,   8800,   8820,   8910,   8960,   9000,   9261,
       9375,   9450,   9504,   9604,   9625,   9702,   9800,  10000,  10125,
      10206,  10240,  10290,  10395,  10500,  10692,  10935,  10976,  11000,
      11025,  11088,  11250,  11264,  11319,  11340,  11550,  11664,  11907,
      12005,  12150,  12250,  12288,  12375,  12474,  12500,  12544,  12672,
      12800,  12936,  13122,  13125,  13230,  13365,  13475,  14000,  14080,
      14112,  14256,  14336,  14400,  14406,  14580,  14700,  14784,  14850,
      15000,  15120,  15360,  15552,  15680,  15750,  15840,  16000,  16128,
      16200,  16384,  16464,  16500,  16632,  16800,  16875,  16896,  17010,
      17280,  17325,  17496,  17500,  17600,  17640,  17820,  17920,  18000,
      18144,  18225,  18432,  18480,  18522,  18750,  18816,  18900,  19008,
      19200,  19440,  19600,  19712,  19800,  20000,  20160,  20480,  20736,
      20790,  21000,  21120,  21168,  21504,  21600,  21870,  21952,  22000,
      22176,  22400,  22500,  22528,  22680,  23040,  23100,  23328,  23520,
      23625,  23760,  24000,  24192,  24300,  24576,  24640,  24696,  24750,
      25000,  25088,  25200,  25344,  25600,  25920,  26244,  26400,  26460,
      26880,  27000,  27216,  27648,  27720,  28000,  28160,  28224,  28512,
      28672,  28800,  28812,  28875,  29160,  29400,  29568,  29700,  30000,
      30240,  30720,  30800,  31104,  31360,  31500,  31680,  31752,  32000,
      32256,  32400,  32768,  32805,  32928,  33000,  33075,  33264,  33600,
      33750,  33792,  34560,  34650,  34992,  35000,  35200,  35280,  35640,
      35840,  36000,  36288,  36864,  36960,  37044,  37125,  37500,  37632,
      37800,  38016,  38400,  38416,  38880,  39200,  39366,  39375,  39424,
      39600,  39690,  40000,  40320,  40500,  40960,  41472,  41580,  42000,
      42240,  42336,  42768,  43008,  43200,  43740,  43904,  44000,  44100,
      44352,  44550,  44800,  45000,  45056,  45360,  46080,  46200,  46656,
      47040,  47250,  47520,  48000,  48384,  48600,  49152,  49280,  49392,
      49500,  50000,  50176,  50400,  50625,  50688,  51200,  51744,  51840,
      51975,  52488,  52500,  52800,  52920,  53760,  54000,  54432,  54675,
      55296,  55440,  56000,  56250,  56320,  56448,  56700,  57024,  57344,
      57600,  57624,  57750,  58320,  58800,  59136,  59400,  60000,  60480,
      60750,  61440,  61740,  62208,  62370,  62720,  63000,  63360,  63504,
      64000,  64512,  64800,  65536,  65610,  66000,  66528,  66825,  67200,
      67500,  67584,  68040,  69120,  69300,  69984,  70400,  70560,  70875,
      71280,  71680,  72000,  72030,  72576,  72900,  73500,  73728,  73920,
      74250,  75000,  75264,  75600,  76032,  76800,  77760,  78400,  78750,
      79200,  79380,  80000,  80190,  80640,  81000,  81648,  81920,  82320,
      82500,  82944,  83160,  84000,  84375,  84480,  84672,  85050,  86016,
      86400,  87480,  87808,  88000,  88200,  88704,  89100,  89600,  90000,
      90720,  92160,  92400,  92610,  93312,  94080,  94500,  95040,  96000,
      96768,  97200,  98304,  98415,  98560,  98784,  99000,  99792, 100000,
     100352, 100800, 101250, 101376, 102060, 102400, 103488, 103680, 103950,
     104976, 105000, 105600, 105840, 106920, 107520, 108000, 108864, 109350,
     110250, 110592, 110880, 111132, 112000, 112500, 112896, 113400, 114048,
     114688, 115200, 115248, 115500, 116640, 117600, 118272, 118800, 120000,
     120960, 122880, 124416, 131072,
     /* }}} */
#else
    /* {{{ Nice transform sizes for simplefft */
          8,      9,     10,     12,     14,     15,     16,     18,     20,
         21,     24,     25,     27,     28,     30,     32,     35,     36,
         40,     42,     45,     48,     50,     54,     56,     60,     63,
         64,     70,     72,     75,     80,     81,     84,     90,     96,
        100,    105,    108,    112,    120,    125,    126,    128,    135,
        140,    144,    150,    160,    162,    168,    175,    180,    189,
        192,    200,    210,    216,    224,    225,    240,    243,    250,
        252,    256,    270,    280,    288,    300,    315,    320,    324,
        336,    350,    360,    375,    378,    384,    400,    405,    420,
        432,    448,    450,    480,    486,    500,    504,    512,    525,
        540,    560,    567,    576,    600,    625,    630,    640,    648,
        672,    675,    700,    720,    729,    750,    756,    768,    800,
        810,    840,    864,    875,    896,    900,    945,    960,    972,
       1000,   1008,   1024,   1050,   1080,   1120,   1125,   1134,   1152,
       1200,   1215,   1250,   1260,   1280,   1296,   1344,   1350,   1400,
       1440,   1458,   1500,   1512,   1536,   1575,   1600,   1620,   1680,
       1701,   1728,   1750,   1792,   1800,   1875,   1890,   1920,   1944,
       2000,   2016,   2025,   2048,   2100,   2160,   2187,   2240,   2250,
       2268,   2304,   2400,   2430,   2500,   2520,   2560,   2592,   2625,
       2688,   2700,   2800,   2835,   2880,   2916,   3000,   3024,   3072,
       3125,   3150,   3200,   3240,   3360,   3375,   3402,   3456,   3500,
       3584,   3600,   3645,   3750,   3780,   3840,   3888,   4000,   4032,
       4050,   4096,   4200,   4320,   4374,   4375,   4480,   4500,   4536,
       4608,   4725,   4800,   4860,   5000,   5040,   5103,   5120,   5184,
       5250,   5376,   5400,   5600,   5625,   5670,   5760,   5832,   6000,
       6048,   6075,   6144,   6250,   6300,   6400,   6480,   6561,   6720,
       6750,   6804,   6912,   7000,   7168,   7200,   7290,   7500,   7560,
       7680,   7776,   7875,   8000,   8064,   8100,   8192,   8400,   8505,
       8640,   8748,   8750,   8960,   9000,   9072,   9216,   9375,   9450,
       9600,   9720,  10000,  10080,  10125,  10206,  10240,  10368,  10500,
      10752,  10800,  10935,  11200,  11250,  11340,  11520,  11664,  12000,
      12096,  12150,  12288,  12500,  12600,  12800,  12960,  13122,  13125,
      13440,  13500,  13608,  13824,  14000,  14175,  14336,  14400,  14580,
      15000,  15120,  15309,  15360,  15552,  15625,  15750,  16000,  16128,
      16200,  16384,  16800,  16875,  17010,  17280,  17496,  17500,  17920,
      18000,  18144,  18225,  18432,  18750,  18900,  19200,  19440,  19683,
      20000,  20160,  20250,  20412,  20480,  20736,  21000,  21504,  21600,
      21870,  21875,  22400,  22500,  22680,  23040,  23328,  23625,  24000,
      24192,  24300,  24576,  25000,  25200,  25515,  25600,  25920,  26244,
      26250,  26880,  27000,  27216,  27648,  28000,  28125,  28350,  28672,
      28800,  29160,  30000,  30240,  30375,  30618,  30720,  31104,  31250,
      31500,  32000,  32256,  32400,  32768,  32805,  33600,  33750,  34020,
      34560,  34992,  35000,  35840,  36000,  36288,  36450,  36864,  37500,
      37800,  38400,  38880,  39366,  39375,  40000,  40320,  40500,  40824,
      40960,  41472,  42000,  42525,  43008,  43200,  43740,  43750,  44800,
      45000,  45360,  45927,  46080,  46656,  46875,  47250,  48000,  48384,
      48600,  49152,  50000,  50400,  50625,  51030,  51200,  51840,  52488,
      52500,  53760,  54000,  54432,  54675,  55296,  56000,  56250,  56700,
      57344,  57600,  58320,  59049,  60000,  60480,  60750,  61236,  61440,
      62208,  62500,  63000,  64000,  64512,  64800,  65536,  65610,  65625,
      67200,  67500,  68040,  69120,  69984,  70000,  70875,  71680,  72000,
      72576,  72900,  73728,  75000,  75600,  76545,  76800,  77760,  78125,
      78732,  78750,  80000,  80640,  81000,  81648,  81920,  82944,  84000,
      84375,  85050,  86016,  86400,  87480,  87500,  89600,  90000,  90720,
      91125,  91854,  92160,  93312,  93750,  94500,  96000,  96768,  97200,
      98304,  98415, 100000, 100800, 101250, 102060, 102400, 103680, 104976,
     105000, 107520, 108000, 108864, 109350, 109375, 110592, 112000, 112500,
     113400, 114688, 115200, 116640, 118098, 118125, 120000, 120960, 121500,
     122472, 122880, 124416, 125000, 126000, 127575, 128000, 129024, 129600,
     131072,
     /* }}} */
#endif
};

static const struct {
    guint i;
    gdouble c;
}
nice_fft_num_2n[] = {
#ifdef HAVE_FFTW3
    { 0,   2.526680,  },
    { 9,   4.111173,  },
    { 24,  5.140072,  },
    { 44,  9.570694,  },
    { 63,  8.475114,  },
    { 89,  8.696155,  },
    { 125, 14.304832, },
    { 172, 18.085240, },
    { 228, 27.128734, },
    { 300, 24.349711, },
    { 379, 37.851200, },
    { 479, 41.963688, },
    { 597, 67.514051, },
    { 714, 0.000000,  },
#else
    { 0,   0.000000,  },
    { 6,   -0.453403, },
    { 15,  0.165279,  },
    { 27,  0.484295,  },
    { 43,  -0.058496, },
    { 64,  1.373230,  },
    { 88,  6.351493,  },
    { 119, 4.749472,  },
    { 156, 6.304195,  },
    { 199, 10.563737, },
    { 249, 11.403761, },
    { 307, 15.693160, },
    { 373, 19.092352, },
    { 447, 24.979771, },
    { 531, 0.000000,  },
#endif
};

/**
 * gwy_fft_find_nice_size:
 * @size: Array size.  Currently it must not be larger than a hard-coded
 *        maximum (64800) which should be large enough for all normal uses.
 *
 * Finds a nice-for-FFT array size.
 *
 * The `nice' means three properties are guaranteed: it is greater than or
 * equal to @size; it can be directly used with current FFT backend without
 * scaling (<link linkend="libgwyprocess-simplefft">simplefft</link> only
 * handles sizes that can be factored into certain small prime factors); and
 * the transform is fast (this is important for FFTW backend which can handle
 * all array sizes, but performance may suffer).
 *
 * Returns: A nice FFT array size.
 **/
gint
gwy_fft_find_nice_size(gint size)
{
    gint i0, i, p0, p1;
    gdouble p;

    /* All numbers smaller than 16 are nice */
    if (size <= nice_fft_num[0])
        return size;
    g_return_val_if_fail(size <= nice_fft_num[G_N_ELEMENTS(nice_fft_num)-1],
                         size);
    /* Find the nearest smaller-or-equal power of 2 */
    for (i = 0, i0 = nice_fft_num[0]; 2*i0 <= size; i++, i0 *= 2)
        ;
    /* Return exact powers of 2 immediately */
    if (i0 == size)
        return size;
    /* Interpolate in the [i0, 2*i0] interval */
    p0 = nice_fft_num_2n[i].i;
    p1 = nice_fft_num_2n[i+1].i;
    p = (size - i0)/(gdouble)i0;
    i = (gint)(p0 + (p1 - p0)*p + nice_fft_num_2n[i].c*(1 - p)*p + 0.5);
    /* Correct the estimated position as we often miss by a number or two */
    while (nice_fft_num[i] < size)
        i++;
    while (nice_fft_num[i-1] >= size)
        i--;

    return nice_fft_num[i];
}

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

    static GArray *buffer = NULL;
    static guint pp[20];  /* 3^21 > G_MAXUINT */

    gdouble *buf_re, *buf_im;
    guint m, np, p, k, bstride;
    gdouble norm_fact;

    if (dir != GWY_TRANSFORM_DIRECTION_BACKWARD) {
        GWY_SWAP(const gdouble*, in_re, in_im);
        GWY_SWAP(gdouble*, out_re, out_im);
    }

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
        else {
            g_critical("%d (%d) contains unimplemented primes", k, n);
            return;
        }
        pp[np] = p;
    }

    /* XXX: This is never freed. */
    if (!buffer)
        buffer = g_array_new(FALSE, FALSE, 2*sizeof(gdouble));
    g_array_set_size(buffer, n);
    buf_re = (gdouble*)buffer->data;
    buf_im = buf_re + n;
    bstride = 1;

    if (np % 2 == 0 && n > 1) {
        GWY_SWAP(gdouble*, buf_re, out_re);
        GWY_SWAP(gdouble*, buf_im, out_im);
        GWY_SWAP(guint, bstride, ostride);
    }

    norm_fact = 1.0/sqrt(n);
    for (m = 0; m < n; m++) {
        out_re[ostride*m] = norm_fact*in_re[istride*m];
        out_im[ostride*m] = norm_fact*in_im[istride*m];
    }

    for (k = 0, m = 1; k < np; k++, m *= p) {
        p = pp[k];
        if (m > 1)
            shuffle_and_twiddle(n, m*p, p,
                                bstride, buf_re, buf_im,
                                ostride, out_re, out_im);
        butterflies[p](n, ostride, out_re, out_im);
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
