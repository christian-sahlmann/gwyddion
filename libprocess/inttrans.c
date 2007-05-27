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

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/inttrans.h>
#include <libprocess/linestats.h>
#include <libprocess/simplefft.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/cwt.h>
#include "gwyprocessinternal.h"

static void  gwy_data_line_fft_do          (GwyDataLine *rsrc,
                                            GwyDataLine *isrc,
                                            GwyDataLine *rdest,
                                            GwyDataLine *idest,
                                            GwyTransformDirection direction);
static void  gwy_data_line_fft_real_do     (GwyDataLine *rsrc,
                                            GwyDataLine *ibuf,
                                            GwyDataLine *rdest,
                                            GwyDataLine *idest,
                                            GwyTransformDirection direction);
static void  gwy_data_field_area_2dfft_real(GwyDataField *ra,
                                            GwyDataField *rb,
                                            GwyDataField *ib,
                                            gint col,
                                            gint row,
                                            gint width,
                                            gint height,
                                            GwyWindowingType windowing,
                                            GwyTransformDirection direction,
                                            GwyInterpolationType interpolation,
                                            gboolean preserverms,
                                            gint level);
static void  gwy_data_field_2dfft_real_do  (GwyDataField *rin,
                                            GwyDataField *ibuf,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_2dfft_do       (GwyDataField *rin,
                                            GwyDataField *iin,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_area_xfft      (GwyDataField *ra,
                                            GwyDataField *ia,
                                            GwyDataField *rb,
                                            GwyDataField *ib,
                                            gint col,
                                            gint row,
                                            gint width,
                                            gint height,
                                            GwyWindowingType windowing,
                                            GwyTransformDirection direction,
                                            GwyInterpolationType interpolation,
                                            gboolean preserverms,
                                            gint level);
static void  gwy_data_field_xfft_do        (GwyDataField *rin,
                                            GwyDataField *iin,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_area_xfft_real (GwyDataField *ra,
                                            GwyDataField *rb,
                                            GwyDataField *ib,
                                            gint col,
                                            gint row,
                                            gint width,
                                            gint height,
                                            GwyWindowingType windowing,
                                            GwyTransformDirection direction,
                                            GwyInterpolationType interpolation,
                                            gboolean preserverms,
                                            gint level);
static void  gwy_data_field_xfft_real_do   (GwyDataField *rin,
                                            GwyDataField *ibuf,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_area_yfft      (GwyDataField *ra,
                                            GwyDataField *ia,
                                            GwyDataField *rb,
                                            GwyDataField *ib,
                                            gint col,
                                            gint row,
                                            gint width,
                                            gint height,
                                            GwyWindowingType windowing,
                                            GwyTransformDirection direction,
                                            GwyInterpolationType interpolation,
                                            gboolean preserverms,
                                            gint level);
static void  gwy_data_field_yfft_do        (GwyDataField *rin,
                                            GwyDataField *iin,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_area_yfft_real (GwyDataField *ra,
                                            GwyDataField *rb,
                                            GwyDataField *ib,
                                            gint col,
                                            gint row,
                                            gint width,
                                            gint height,
                                            GwyWindowingType windowing,
                                            GwyTransformDirection direction,
                                            GwyInterpolationType interpolation,
                                            gboolean preserverms,
                                            gint level);
static void  gwy_data_field_yfft_real_do   (GwyDataField *rin,
                                            GwyDataField *ibuf,
                                            GwyDataField *rout,
                                            GwyDataField *iout,
                                            GwyTransformDirection direction);
static void  gwy_data_field_mult_wav       (GwyDataField *real_field,
                                            GwyDataField *imag_field,
                                            gdouble scale,
                                            Gwy2DCWTWaveletType wtype);
static void  gwy_level_simple              (gint n,
                                            gint stride,
                                            gdouble *data,
                                            gint level);
static void  gwy_preserve_rms_simple       (gint nsrc,
                                            gint stridesrc,
                                            const gdouble *src1,
                                            const gdouble *src2,
                                            gint ndata,
                                            gint stridedata,
                                            gdouble *data1,
                                            gdouble *data2);

#ifdef HAVE_FFTW3
/* Good FFTW array sizes for extended and resampled arrays.
 *
 * Since extending and resampling always involves an O(N) part -- N being the
 * extended array size -- that may even be dominant, it isn't wise to use the
 * fastest possible FFT if it requires considerably larger array.  Following
 * numbers represent a reasonable compromise tested on a few platforms */
static const guint nice_fftw_num[] = {
        16,     18,     20,     21,     22,     24,     25,     27,     28,
        32,     33,     35,     36,     40,     42,     44,     45,     48,
        49,     50,     54,     55,     56,     60,     64,     66,     70,
        72,     75,     77,     80,     81,     84,     90,     96,     98,
        99,    100,    105,    108,    110,    112,    120,    126,    128,
       132,    135,    140,    144,    150,    160,    165,    168,    176,
       180,    192,    196,    198,    200,    216,    224,    225,    240,
       256,    264,    280,    288,    294,    300,    320,    324,    330,
       336,    343,    350,    352,    360,    384,    385,    392,    396,
       400,    405,    420,    432,    448,    450,    480,    512,    525,
       528,    540,    550,    576,    588,    594,    600,    640,    648,
       660,    672,    675,    686,    700,    704,    720,    735,    768,
       770,    784,    792,    800,    810,    825,    840,    864,    880,
       896,    900,    924,    960,    972,    980,   1008,   1024,   1029,
      1056,   1080,   1100,   1120,   1125,   1134,   1155,   1176,   1188,
      1200,   1215,   1232,   1260,   1280,   1296,   1344,   1350,   1372,
      1400,   1408,   1440,   1470,   1500,   1512,   1536,   1540,   1568,
      1575,   1584,   1600,   1620,   1650,   1680,   1728,   1760,   1764,
      1792,   1800,   1848,   1890,   1920,   1944,   1960,   2000,   2016,
      2048,   2058,   2100,   2112,   2160,   2200,   2205,   2240,   2250,
      2304,   2310,   2352,   2400,   2430,   2450,   2464,   2475,   2520,
      2560,   2592,   2640,   2688,   2700,   2744,   2772,   2800,   2816,
      2880,   2916,   2940,   3000,   3024,   3072,   3136,   3150,   3168,
      3200,   3240,   3300,   3360,   3375,   3456,   3500,   3520,   3528,
      3584,   3600,   3645,   3675,   3696,   3750,   3780,   3840,   3888,
      3920,   4032,   4096,   4116,   4125,   4158,   4200,   4224,   4320,
      4374,   4400,   4410,   4480,   4500,   4608,   4620,   4704,   4725,
      4752,   4800,   4802,   4860,   4900,   4928,   5000,   5040,   5120,
      5184,   5250,   5280,   5376,   5400,   5488,   5500,   5544,   5600,
      5625,   5632,   5670,   5760,   5832,   5880,   6000,   6048,   6144,
      6160,   6272,   6300,   6336,   6400,   6480,   6600,   6615,   6720,
      6750,   6912,   7000,   7040,   7056,   7168,   7200,   7203,   7290,
      7350,   7392,   7425,   7500,   7546,   7700,   7875,   7938,   8000,
      8019,   8085,   8192,   8232,   8250,   8316,   8505,   8575,   8624,
      8748,   8750,   8800,   8820,   8910,   8960,   9000,   9261,   9375,
      9450,   9504,   9604,   9625,   9702,   9800,  10000,  10125,  10206,
     10240,  10290,  10395,  10500,  10692,  10935,  10976,  11000,  11025,
     11088,  11250,  11264,  11319,  11340,  11550,  11664,  11907,  12005,
     12150,  12250,  12288,  12375,  12474,  12500,  12544,  12672,  12800,
     12936,  13122,  13125,  13230,  13365,  13475,  14000,  14080,  14112,
     14256,  14336,  14400,  14406,  14580,  14700,  14784,  14850,  15000,
     15120,  15360,  15552,  15680,  15750,  15840,  16000,  16128,  16200,
     16384,  16464,  16500,  16632,  16800,  16875,  16896,  17010,  17280,
     17325,  17496,  17500,  17600,  17640,  17820,  17920,  18000,  18144,
     18225,  18432,  18480,  18522,  18750,  18816,  18900,  19008,  19200,
     19440,  19600,  19712,  19800,  20000,  20160,  20480,  20736,  20790,
     21000,  21120,  21168,  21504,  21600,  21870,  21952,  22000,  22176,
     22400,  22500,  22528,  22680,  23040,  23100,  23328,  23520,  23625,
     23760,  24000,  24192,  24300,  24576,  24640,  24696,  24750,  25000,
     25088,  25200,  25344,  25600,  25920,  26244,  26400,  26460,  26880,
     27000,  27216,  27648,  27720,  28000,  28160,  28224,  28512,  28672,
     28800,  28812,  28875,  29160,  29400,  29568,  29700,  30000,  30240,
     30720,  30800,  31104,  31360,  31500,  31680,  31752,  32000,  32256,
     32400,  32768,  32805,  32928,  33000,  33075,  33264,  33600,  33750,
     33792,  34560,  34650,  34992,  35000,  35200,  35280,  35640,  35840,
     36000,  36288,  36864,  36960,  37044,  37125,  37500,  37632,  37800,
     38016,  38400,  38416,  38880,  39200,  39366,  39375,  39424,  39600,
     39690,  40000,  40320,  40500,  40960,  41472,  41580,  42000,  42240,
     42336,  42768,  43008,  43200,  43740,  43904,  44000,  44100,  44352,
     44550,  44800,  45000,  45056,  45360,  46080,  46200,  46656,  47040,
     47250,  47520,  48000,  48384,  48600,  49152,  49280,  49392,  49500,
     50000,  50176,  50400,  50625,  50688,  51200,  51744,  51840,  51975,
     52488,  52500,  52800,  52920,  53760,  54000,  54432,  54675,  55296,
     55440,  56000,  56250,  56320,  56448,  56700,  57024,  57344,  57600,
     57624,  57750,  58320,  58800,  59136,  59400,  60000,  60480,  60750,
     61440,  61740,  62208,  62370,  62720,  63000,  63360,  63504,  64000,
     64512,  64800,  65536,  65610,  66000,  66528,  66825,  67200,  67500,
     67584,  68040,  69120,  69300,  69984,  70400,  70560,  70875,  71280,
     71680,  72000,  72030,  72576,  72900,  73500,  73728,  73920,  74250,
     75000,  75264,  75600,  76032,  76800,  77760,  78400,  78750,  79200,
     79380,  80000,  80190,  80640,  81000,  81648,  81920,  82320,  82500,
     82944,  83160,  84000,  84375,  84480,  84672,  85050,  86016,  86400,
     87480,  87808,  88000,  88200,  88704,  89100,  89600,  90000,  90720,
     92160,  92400,  92610,  93312,  94080,  94500,  95040,  96000,  96768,
     97200,  98304,  98415,  98560,  98784,  99000,  99792, 100000, 100352,
    100800, 101250, 101376, 102060, 102400, 103488, 103680, 103950, 104976,
    105000, 105600, 105840, 106920, 107520, 108000, 108864, 109350, 110250,
    110592, 110880, 111132, 112000, 112500, 112896, 113400, 114048, 114688,
    115200, 115248, 115500, 116640, 117600, 118272, 118800, 120000, 120960,
    122880, 124416, 131072,
};

static const struct {
    guint i;
    gdouble c;
}
nice_fftw_num_2n[] = {
    { 0,   2.526680,  },
    { 9,   4.111173,  },
    { 24,  5.140072,  },
    { 44,  9.570694,  },
    { 63,  10.625654, },
    { 88,  8.696155,  },
    { 124, 14.304832, },
    { 171, 18.085240, },
    { 227, 27.128734, },
    { 299, 24.349711, },
    { 378, 37.851200, },
    { 478, 41.963688, },
    { 596, 67.514051, },
    { 713, 0.000000,  },
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
 * scaling (<link linkend="libgwyprocess-simplefft">simplefft</link> can only
 * handle powers of 2); and the transform is fast (this is important for FFTW
 * backend which can handle all array sizes, but performance may suffer).
 *
 * Returns: A nice FFT array size.
 **/
gint
gwy_fft_find_nice_size(gint size)
{
    gint i0, i, p0, p1;
    gdouble p;

    /* All numbers smaller than 16 are nice */
    if (size <= nice_fftw_num[0])
        return size;
    g_return_val_if_fail(size <= nice_fftw_num[G_N_ELEMENTS(nice_fftw_num)-1],
                         size);
    /* Find the nearest smaller-or-equal power of 2 */
    for (i = 0, i0 = nice_fftw_num[0]; 2*i0 <= size; i++, i0 *= 2)
        ;
    /* Return exact powers of 2 immediately */
    if (i0 == size)
        return size;
    /* Interpolate in the [i0, 2*i0] interval */
    p0 = nice_fftw_num_2n[i].i;
    p1 = nice_fftw_num_2n[i+1].i;
    p = (size - i0)/(gdouble)i0;
    i = (gint)(p0 + (p1 - p0)*p + nice_fftw_num_2n[i].c*(1 - p)*p + 0.5);
    /* Correct the estimated position as we often miss by a number or two */
    while (nice_fftw_num[i] < size)
        i++;
    while (nice_fftw_num[i-1] >= size)
        i--;

    return nice_fftw_num[i];
}
#else  /* HAVE_FFTW3 */
gint
gwy_fft_find_nice_size(gint size)
{
    gint p2;

    g_return_val_if_fail(size <= 0x40000000, 0x40000000);

    for (p2 = 1; p2 < size; p2 = p2 << 1)
        ;

    return p2;
}
#endif  /* HAVE_FFTW3 */

/**
 * gwy_data_line_fft:
 * @rsrc: Real input data line.
 * @isrc: Imaginary input data line.
 * @rdest: Real output data line.  It will be resized to the size of the input
 *         data line.
 * @idest: Imaginary output data line.  It will be resized to the size of the
 *         input data line.
 * @windowing: Windowing mode.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS value while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         line (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Calculates Fast Fourier Transform of a data line.
 *
 * A windowing or data leveling can be applied if requested.
 **/
void
gwy_data_line_fft(GwyDataLine *rsrc, GwyDataLine *isrc,
                  GwyDataLine *rdest, GwyDataLine *idest,
                  GwyWindowingType windowing,
                  GwyTransformDirection direction,
                  GwyInterpolationType interpolation,
                  gboolean preserverms,
                  gint level)
{
    g_return_if_fail(GWY_IS_DATA_LINE(rsrc));

    gwy_data_line_part_fft(rsrc, isrc, rdest, idest,
                           0, rsrc->res,
                           windowing, direction, interpolation,
                           preserverms, level);
}

/**
 * gwy_data_line_part_fft:
 * @rsrc: Real input data line.
 * @isrc: Imaginary input data line. Since 2.6 it can be %NULL for
 *        real-to-complex transforms.
 * @rdest: Real output data line, it will be resized to @len.
 * @idest: Imaginary output data line, it will be resized to @len.
 * @from: The index in input lines to start from (inclusive).
 * @len: Lenght of data line part, it must be at least 4.
 * @windowing: Windowing mode.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS value while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         line (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Calculates Fast Fourier Transform of a part of a data line.
 *
 * A windowing or data leveling can be applied if requested.
 **/
void
gwy_data_line_part_fft(GwyDataLine *rsrc, GwyDataLine *isrc,
                       GwyDataLine *rdest, GwyDataLine *idest,
                       gint from, gint len,
                       GwyWindowingType windowing,
                       GwyTransformDirection direction,
                       GwyInterpolationType interpolation,
                       gboolean preserverms,
                       gint level)
{
    gint newres;
    GwyDataLine *rbuf, *ibuf;

    g_return_if_fail(GWY_IS_DATA_LINE(rsrc));
    g_return_if_fail(!isrc || GWY_IS_DATA_LINE(isrc));
    if (isrc)
        g_return_if_fail(!gwy_data_line_check_compatibility
                                     (rsrc, isrc, GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail(GWY_IS_DATA_LINE(rdest));
    g_return_if_fail(GWY_IS_DATA_LINE(idest));
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(from >= 0
                     && len >= 4
                     && from + len <= rsrc->res);

    newres = gwy_fft_find_nice_size(rsrc->res);

    gwy_data_line_resample(rdest, newres, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(idest, newres, GWY_INTERPOLATION_NONE);

    rbuf = gwy_data_line_part_extract(rsrc, from, len);
    gwy_level_simple(len, 1, rbuf->data, level);
    gwy_fft_window(len, rbuf->data, windowing);
    gwy_data_line_resample(rbuf, newres, interpolation);

    if (isrc) {
        ibuf = gwy_data_line_part_extract(isrc, from, len);
        gwy_level_simple(len, 1, ibuf->data, level);
        gwy_fft_window(len, ibuf->data, windowing);
        gwy_data_line_resample(ibuf, newres, interpolation);
        gwy_data_line_fft_do(rbuf, ibuf, rdest, idest, direction);
        if (preserverms)
            gwy_preserve_rms_simple(len, 1,
                                    rsrc->data + from, isrc->data + from,
                                    newres, 1,
                                    rdest->data, idest->data);
    }
    else {
        ibuf = gwy_data_line_new_alike(rbuf, FALSE);
        gwy_data_line_fft_real_do(rbuf, ibuf, rdest, idest, direction);
        if (preserverms)
            gwy_preserve_rms_simple(len, 1,
                                    rsrc->data + from, NULL,
                                    newres, 1,
                                    rdest->data, idest->data);
    }

    gwy_data_line_resample(rdest, len, interpolation);
    gwy_data_line_resample(idest, len, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

/**
 * gwy_data_line_fft_raw:
 * @rsrc: Real input data line.
 * @isrc: Imaginary input data line.  Since 2.6 it can be %NULL for
 *        real-to-complex transform.
 * @rdest: Real output data line.  It will be resized to the size of the input
 *         data line.
 * @idest: Imaginary output data line.  It will be resized to the size of the
 *         input data line.
 * @direction: FFT direction.
 *
 * Calculates Fast Fourier Transform of a data line.
 *
 * The resolutions of @rsrc and @isrc have to be from the set
 * of sizes returned by gwy_fft_find_nice_size().  No leveling, windowing nor
 * scaling is performed.
 *
 * Since: 2.1
 **/
void
gwy_data_line_fft_raw(GwyDataLine *rsrc,
                      GwyDataLine *isrc,
                      GwyDataLine *rdest,
                      GwyDataLine *idest,
                      GwyTransformDirection direction)
{
    gint newres;

    g_return_if_fail(GWY_IS_DATA_LINE(rsrc));
    g_return_if_fail(!isrc || GWY_IS_DATA_LINE(isrc));
    if (isrc)
        g_return_if_fail(!gwy_data_line_check_compatibility
                                     (rsrc, isrc, GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail(GWY_IS_DATA_LINE(rdest));
    g_return_if_fail(GWY_IS_DATA_LINE(idest));
    newres = gwy_fft_find_nice_size(rsrc->res);
    g_return_if_fail(rsrc->res == newres);

    gwy_data_line_resample(rdest, newres, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(idest, newres, GWY_INTERPOLATION_NONE);

    if (isrc)
        g_object_ref(isrc);
    else
        isrc = gwy_data_line_new_alike(rsrc, TRUE);

    gwy_data_line_fft_do(rsrc, isrc, rdest, idest, direction);
    g_object_unref(isrc);
}

static void
gwy_data_line_fft_do(GwyDataLine *rsrc,
                     GwyDataLine *isrc,
                     GwyDataLine *rdest,
                     GwyDataLine *idest,
                     GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;

    dims[0].n = rsrc->res;
    dims[0].is = 1;
    dims[0].os = 1;
    howmany_dims[0].n = 1;
    howmany_dims[0].is = rsrc->res;
    howmany_dims[0].os = rsrc->res;
    /* Backward direction is equivalent to switching real and imaginary parts */
    /* XXX: Planner destroys input, we have to either allocate memory or
     * use in-place transform.  In some cases caller could provide us with
     * already allocated buffers. */
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        rdest->data, idest->data,
                                        rdest->data, idest->data,
                                        _GWY_FFTW_PATIENCE);
    else
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        idest->data, rdest->data,
                                        idest->data, rdest->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    gwy_data_line_copy(rsrc, rdest);
    gwy_data_line_copy(isrc, idest);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    gwy_data_line_multiply(rdest, 1.0/sqrt(rsrc->res));
    gwy_data_line_multiply(idest, 1.0/sqrt(rsrc->res));
#else
    gwy_fft_simple(direction, rsrc->res,
                   1, rsrc->data, isrc->data,
                   1, rdest->data, idest->data);
#endif
}

static void
gwy_data_line_fft_real_do(GwyDataLine *rsrc,
                          GwyDataLine *ibuf,
                          GwyDataLine *rdest,
                          GwyDataLine *idest,
                          GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;
    gint j;

    dims[0].n = rsrc->res;
    dims[0].is = 1;
    dims[0].os = 1;
    howmany_dims[0].n = 1;
    howmany_dims[0].is = rsrc->res;
    howmany_dims[0].os = rsrc->res;
    /* Backward direction is equivalent to switching real and imaginary parts */
    plan = fftw_plan_guru_split_dft_r2c(1, dims, 1, howmany_dims,
                                        ibuf->data,
                                        rdest->data, idest->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    /* R2C destroys input, and especially, the planner destroys input too */
    gwy_data_line_copy(rsrc, ibuf);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    /* Complete the missing half of transform.  */
    for (j = rsrc->res/2 + 1; j < rsrc->res; j++) {
        rdest->data[j] = rdest->data[rsrc->res - j];
        idest->data[j] = -idest->data[rsrc->res - j];
    }

    gwy_data_line_multiply(rdest, 1.0/sqrt(rsrc->res));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_line_multiply(idest, 1.0/sqrt(rsrc->res));
    else
        gwy_data_line_multiply(idest, -1.0/sqrt(rsrc->res));
#else
    /* We cannot save anything here.  Or correct me... */
    gwy_data_line_clear(ibuf);
    gwy_fft_simple(direction, rsrc->res,
                   1, rsrc->data, ibuf->data,
                   1, rdest->data, idest->data);
#endif
}

static void
gwy_data_field_2dfft_prepare(GwyDataField *dfield,
                             gint level,
                             GwyWindowingType windowing,
                             gboolean preserverms,
                             gdouble *rms)
{
    gdouble a, bx, by;

    if (level == 2) {
        gwy_data_field_fit_plane(dfield, &a, &bx, &by);
        gwy_data_field_plane_level(dfield, a, bx, by);
    }
    else if (level == 1)
        gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    if (preserverms) {
        a = gwy_data_field_get_rms(dfield);
        *rms = hypot(*rms, a);
    }
    gwy_fft_window_data_field(dfield, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_data_field(dfield, GWY_ORIENTATION_VERTICAL, windowing);
}

/**
 * gwy_data_field_area_2dfft:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform which can be somewhat faster than complex-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         plane (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Calculates 2D Fast Fourier Transform of a rectangular area of a data field.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_area_2dfft(GwyDataField *rin, GwyDataField *iin,
                          GwyDataField *rout, GwyDataField *iout,
                          gint col, gint row,
                          gint width, gint height,
                          GwyWindowingType windowing,
                          GwyTransformDirection direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms, gint level)
{
    gint j, k, newxres, newyres;
    GwyDataField *rbuf, *ibuf;
    gdouble *out_rdata, *out_idata;
    gdouble rmsa = 0.0, rmsb;

    if (!iin) {
        gwy_data_field_area_2dfft_real(rin, rout, iout,
                                       col, row, width, height,
                                       windowing, direction, interpolation,
                                       preserverms, level);
        return;
    }

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iin));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(rin->xres == iin->xres && rin->yres == rout->yres);
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newxres = gwy_fft_find_nice_size(width);
    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rout, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rout->data;

    gwy_data_field_resample(iout, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_idata = iout->data;

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    gwy_data_field_2dfft_prepare(rbuf, level, windowing, preserverms, &rmsa);
    gwy_data_field_resample(rbuf, newxres, newyres, interpolation);

    ibuf = gwy_data_field_area_extract(iin, col, row, width, height);
    gwy_data_field_2dfft_prepare(ibuf, level, windowing, preserverms, &rmsa);
    gwy_data_field_resample(ibuf, newxres, newyres, interpolation);

    gwy_data_field_2dfft_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        /* Ignore coefficient [0,0] */
        rmsb = -(out_rdata[0]*out_rdata[0] + out_idata[0]*out_idata[0]);
        for (j = 0; j < newyres; j++) {
            for (k = 0; k < newxres; k++)
                rmsb += (out_rdata[j*newxres + k]*out_rdata[j*newxres + k]
                         + out_idata[j*newxres + k]*out_idata[j*newxres + k]);
        }
        rmsb = sqrt(rmsb)/(newxres*newyres);
        if (rmsb > 0.0) {
            gwy_data_field_multiply(rout, rmsa/rmsb);
            gwy_data_field_multiply(iout, rmsa/rmsb);
        }
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_2dfft_raw:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to @rin size.
 * @iout: Imaginary output data field, it will be resized to @rin size.
 * @direction: FFT direction.
 *
 * Calculates 2D Fast Fourier Transform of a data field.
 *
 * The resolutions of @rin and @iin have to be from the set
 * of sizes returned by gwy_fft_find_nice_size().  No leveling, windowing nor
 * scaling is performed.
 *
 * Since: 2.1
 **/
void
gwy_data_field_2dfft_raw(GwyDataField *rin,
                         GwyDataField *iin,
                         GwyDataField *rout,
                         GwyDataField *iout,
                         GwyTransformDirection direction)
{
    gint newxres, newyres;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(!iin || GWY_IS_DATA_FIELD(iin));
    if (iin)
        g_return_if_fail(!gwy_data_field_check_compatibility
                                       (rin, iin, GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    newxres = gwy_fft_find_nice_size(rin->xres);
    g_return_if_fail(rin->xres == newxres);
    newyres = gwy_fft_find_nice_size(rin->yres);
    g_return_if_fail(rin->yres == newyres);

    gwy_data_field_resample(rout, newxres, newyres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(iout, newxres, newyres, GWY_INTERPOLATION_NONE);

    if (iin)
        gwy_data_field_2dfft_do(rin, iin, rout, iout, direction);
    else {
        iin = gwy_data_field_new_alike(rin, FALSE);
        gwy_data_field_2dfft_real_do(rin, iin, rout, iout, direction);
        g_object_unref(iin);
    }
}

static void
gwy_data_field_2dfft_do(GwyDataField *rin,
                        GwyDataField *iin,
                        GwyDataField *rout,
                        GwyDataField *iout,
                        GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[2], howmany_dims[1];
    fftw_plan plan;

    dims[1].n = rin->xres;
    dims[1].is = 1;
    dims[1].os = 1;
    dims[0].n = rin->yres;
    dims[0].is = dims[1].is * dims[1].n;
    dims[0].os = dims[1].os * dims[1].n;
    howmany_dims[0].n = 1;
    howmany_dims[0].is = rin->xres*rin->yres;
    howmany_dims[0].os = rin->xres*rin->yres;
    /* Backward direction is equivalent to switching real and imaginary parts */
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        plan = fftw_plan_guru_split_dft(2, dims, 1, howmany_dims,
                                        rin->data, iin->data,
                                        rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    else
        plan = fftw_plan_guru_split_dft(2, dims, 1, howmany_dims,
                                        iin->data, rin->data,
                                        iout->data, rout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->xres*rin->yres));
    gwy_data_field_multiply(iout, 1.0/sqrt(rin->xres*rin->yres));
#else
    gdouble *ibuf, *rbuf;
    gint j, k, xres, yres;

    xres = rin->xres;
    yres = rin->yres;
    for (k = 0; k < yres; k++) {
        gwy_fft_simple(direction, xres,
                       1, rin->data + k*xres, iin->data + k*xres,
                       1, rout->data + k*xres, iout->data + k*xres);
    }
    /* Use a one-row temporary buffer */
    rbuf = g_new(gdouble, 2*yres);
    ibuf = rbuf + 1;
    for (k = 0; k < xres; k++) {
        gwy_fft_simple(direction, yres,
                       xres, rout->data + k, iout->data + k,
                       2, rbuf, ibuf);
        /* Move the result from buffer to iout, rout columns */
        for (j = 0; j < yres; j++) {
            rout->data[j*xres + k] = rbuf[2*j];
            iout->data[j*xres + k] = ibuf[2*j];
        }
    }
    g_free(rbuf);
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_area_2dfft_real:
 * @rin: Real input data field.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         plane (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Calculates 2D Fast Fourier Transform of a rectangular area of a data field.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_2dfft().
 **/
static void
gwy_data_field_area_2dfft_real(GwyDataField *rin,
                               GwyDataField *rout, GwyDataField *iout,
                               gint col, gint row,
                               gint width, gint height,
                               GwyWindowingType windowing,
                               GwyTransformDirection direction,
                               GwyInterpolationType interpolation,
                               gboolean preserverms, gint level)
{
    gint newxres, newyres, j, k;
    GwyDataField *rbuf, *ibuf;
    gdouble *out_rdata, *out_idata;
    gdouble rmsa = 0.0, rmsb;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newxres = gwy_fft_find_nice_size(width);
    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rout, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rout->data;

    gwy_data_field_resample(iout, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_idata = iout->data;

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    gwy_data_field_2dfft_prepare(rbuf, level, windowing, preserverms, &rmsa);
    gwy_data_field_resample(rbuf, newxres, newyres, interpolation);

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);

    gwy_data_field_2dfft_real_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        /* Ignore coefficient [0,0] */
        rmsb = -(out_rdata[0]*out_rdata[0] + out_idata[0]*out_idata[0]);
        for (j = 0; j < newyres; j++) {
            for (k = 0; k < newxres; k++)
                rmsb += (out_rdata[j*newxres + k]*out_rdata[j*newxres + k]
                         + out_idata[j*newxres + k]*out_idata[j*newxres + k]);
        }
        rmsb = sqrt(rmsb)/(newxres*newyres);
        if (rmsb > 0.0) {
            gwy_data_field_multiply(rout, rmsa/rmsb);
            gwy_data_field_multiply(iout, rmsa/rmsb);
        }
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

static void
gwy_data_field_2dfft_real_do(GwyDataField *rin,
                             GwyDataField *ibuf,
                             GwyDataField *rout,
                             GwyDataField *iout,
                             GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[2], howmany_dims[1];
    fftw_plan plan;
    gint j, k;

    dims[1].n = rin->xres;
    dims[1].is = 1;
    dims[1].os = 1;
    dims[0].n = rin->yres;
    dims[0].is = dims[1].is * dims[1].n;
    dims[0].os = dims[1].os * dims[1].n;
    howmany_dims[0].n = 1;
    howmany_dims[0].is = rin->xres*rin->yres;
    howmany_dims[0].os = rin->xres*rin->yres;
    plan = fftw_plan_guru_split_dft_r2c(2, dims, 1, howmany_dims,
                                        ibuf->data, rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    /* R2C destroys input, and especially, the planner destroys input too */
    gwy_data_field_copy(rin, ibuf, FALSE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    /* Complete the missing half of transform.  */
    for (j = rin->xres/2 + 1; j < rin->xres; j++) {
        rout->data[j] = rout->data[rin->xres - j];
        iout->data[j] = -iout->data[rin->xres - j];
    }
    for (k = 1; k < rin->yres; k++) {
        gdouble *r0, *i0, *r1, *i1;

        r0 = rout->data + k*rin->xres;
        i0 = iout->data + k*rin->xres;
        r1 = rout->data + (rin->yres - k)*rin->xres;
        i1 = iout->data + (rin->yres - k)*rin->xres;
        for (j = rin->xres/2 + 1; j < rin->xres; j++) {
            r0[j] = r1[rin->xres - j];
            i0[j] = -i1[rin->xres - j];
        }
    }

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->xres*rin->yres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(iout, 1.0/sqrt(rin->xres*rin->yres));
    else
        gwy_data_field_multiply(iout, -1.0/sqrt(rin->xres*rin->yres));
#else
    gint k, j;

    for (k = 0; k+1 < rin->yres; k += 2) {
        gdouble *re, *im, *r0, *r1, *i0, *i1;

        re = rout->data + k*rin->xres;
        im = rout->data + (k + 1)*rin->xres;
        r0 = rin->data + k*rin->xres;
        r1 = rin->data + (k + 1)*rin->xres;
        i0 = ibuf->data + k*rin->xres;
        i1 = ibuf->data + (k + 1)*rin->xres;

        gwy_fft_simple(direction, rin->xres, 1, r0, r1, 1, re, im);

        /* Disentangle transforms of the row couples */
        r0[0] = re[0];
        i0[0] = 0.0;
        r1[0] = im[0];
        i1[0] = 0.0;
        for (j = 1; j < rin->xres; j++) {
            r0[j] = (re[j] + re[rin->xres - j])/2.0;
            i0[j] = (im[j] - im[rin->xres - j])/2.0;
            r1[j] = (im[j] + im[rin->xres - j])/2.0;
            i1[j] = (-re[j] + re[rin->xres - j])/2.0;
        }
    }
    /* XXX: Maybe this is paranoid here as in 2D FFT also the other dimension
     * has to be a nice number -- power of 2 here */
    if (rin->yres % 2) {
        k = rin->xres * (rin->yres - 1);
        memset(ibuf->data, 0, rin->xres*sizeof(gdouble));
        gwy_fft_simple(direction, rin->xres,
                       1, rin->data + k, ibuf->data,
                       1, rout->data + k, iout->data + k);

    }
    for (k = 0; k < rin->xres; k++)
        gwy_fft_simple(direction, rin->yres,
                       rin->xres, rin->data + k, ibuf->data + k,
                       rin->xres, rout->data + k, iout->data + k);
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_2dfft:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform which can be somewhat faster than complex-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         plane (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Calculates 2D Fast Fourier Transform of a rectangular a data field.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_2dfft(GwyDataField *rin, GwyDataField *iin,
                     GwyDataField *rout, GwyDataField *iout,
                     GwyWindowingType windowing,
                     GwyTransformDirection direction,
                     GwyInterpolationType interpolation,
                     gboolean preserverms, gint level)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(rin));

    if (!iin)
        gwy_data_field_area_2dfft_real(rin, rout, iout,
                                       0, 0, rin->xres, rin->yres,
                                       windowing, direction, interpolation,
                                       preserverms, level);
    else
        gwy_data_field_area_2dfft(rin, iin, rout, iout,
                                  0, 0, rin->xres, rin->yres,
                                  windowing, direction, interpolation,
                                  preserverms, level);
}

/**
 * gwy_data_field_2dfft_humanize:
 * @data_field: A data field.
 *
 * Rearranges 2D FFT output to a human-friendly form.
 *
 * Top-left, top-right, bottom-left and bottom-right sub-squares are swapped
 * to obtain a humanized 2D FFT output with (0,0) in the center.
 **/
void
gwy_data_field_2dfft_humanize(GwyDataField *data_field)
{
    GwyDataField *tmp;
    gint i, j, im, jm, xres, yres;
    gdouble *data;

    im = data_field->yres/2;
    jm = data_field->xres/2;
    xres = data_field->xres;
    yres = data_field->yres;

    /* When both dimensions are even, we can simply swap the data without
     * allocation of temporary buffers. */
    if (xres == 2*jm && yres == 2*im) {
        data = data_field->data;

        for (i = 0; i < im; i++) {
            for (j = 0; j < jm; j++) {
                GWY_SWAP(gdouble,
                         data[j + i*xres], data[(j + jm) + (i + im)*xres]);
                GWY_SWAP(gdouble,
                         data[j + (i + im)*xres], data[(j + jm) + i*xres]);
            }
        }
        gwy_data_field_invalidate(data_field);

        return;
    }

    tmp = gwy_data_field_new_alike(data_field, FALSE);
    /* FIXME: this had looked like using the new gwy_data_field_area_copy()
     * argument convention *before* I changed it.  So either it was buggy
     * or it is buggy now. */
    gwy_data_field_area_copy(data_field, tmp, 0, 0, xres-jm, yres-im, jm, im);
    gwy_data_field_area_copy(data_field, tmp, xres-jm, 0, jm, yres-im, 0, im);
    gwy_data_field_area_copy(data_field, tmp, 0, yres-im, xres-jm, im, jm, 0);
    gwy_data_field_area_copy(data_field, tmp, xres-jm, yres-im, jm, im, 0, 0);
    gwy_data_field_copy(tmp, data_field, FALSE);
    g_object_unref(tmp);
}

/**
 * gwy_data_field_area_1dfft:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform which can be somewhat faster than complex-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4 for horizontal
 *         transforms.
 * @height: Area height (number of rows), must be at least 4 for vertical
 *          transforms.
 * @orientation: Orientation: pass %GWY_ORIENTATION_HORIZONTAL to
 *               transform rows, %GWY_ORIENTATION_VERTICAL to transform
 *               columns.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         lines (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all rows or columns in a rectangular part of a data field with
 * Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_area_1dfft(GwyDataField *rin, GwyDataField *iin,
                          GwyDataField *rout, GwyDataField *iout,
                          gint col, gint row,
                          gint width, gint height,
                          GwyOrientation orientation,
                          GwyWindowingType windowing,
                          GwyTransformDirection direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms,
                          gint level)
{
    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        if (!iin)
            gwy_data_field_area_xfft_real(rin, rout, iout,
                                          col, row, width, height,
                                          windowing, direction, interpolation,
                                          preserverms, level);
        else
            gwy_data_field_area_xfft(rin, iin, rout, iout,
                                     col, row, width, height,
                                     windowing, direction, interpolation,
                                     preserverms, level);
        break;

        case GWY_ORIENTATION_VERTICAL:
        if (!iin)
            gwy_data_field_area_yfft_real(rin, rout, iout,
                                          col, row, width, height,
                                          windowing, direction, interpolation,
                                          preserverms, level);
        else
            gwy_data_field_area_yfft(rin, iin, rout, iout,
                                     col, row, width, height,
                                     windowing, direction, interpolation,
                                     preserverms, level);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_data_field_1dfft:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform which can be somewhat faster than complex-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @orientation: Orientation: pass %GWY_ORIENTATION_HORIZONTAL to
 *               transform rows, %GWY_ORIENTATION_VERTICAL to transform
 *               columns.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         line (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all rows or columns in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_1dfft(GwyDataField *rin, GwyDataField *iin,
                     GwyDataField *rout, GwyDataField *iout,
                     GwyOrientation orientation,
                     GwyWindowingType windowing,
                     GwyTransformDirection direction,
                     GwyInterpolationType interpolation,
                     gboolean preserverms,
                     gint level)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(rin));

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        if (!iin)
            gwy_data_field_area_xfft_real(rin, rout, iout,
                                          0, 0, rin->xres, rin->yres,
                                          windowing, direction, interpolation,
                                          preserverms, level);
        else
            gwy_data_field_area_xfft(rin, iin, rout, iout,
                                     0, 0, rin->xres, rin->yres,
                                     windowing, direction, interpolation,
                                     preserverms, level);
        break;

        case GWY_ORIENTATION_VERTICAL:
        if (!iin)
            gwy_data_field_area_yfft_real(rin, rout, iout,
                                          0, 0, rin->xres, rin->yres,
                                          windowing, direction, interpolation,
                                          preserverms, level);
        else
            gwy_data_field_area_yfft(rin, iin, rout, iout,
                                     0, 0, rin->xres, rin->yres,
                                     windowing, direction, interpolation,
                                     preserverms, level);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_data_field_1dfft_raw:
 * @rin: Real input data field.
 * @iin: Imaginary input data field.  It can be %NULL for real-to-complex
 *       transform.
 * @rout: Real output data field, it will be resized to @rin size.
 * @iout: Imaginary output data field, it will be resized to @rin size.
 * @orientation: Orientation: pass %GWY_ORIENTATION_HORIZONTAL to
 *               transform rows, %GWY_ORIENTATION_VERTICAL to transform
 *               columns.
 * @direction: FFT direction.
 *
 * Transforms all rows or columns in a data field with Fast Fourier Transform.
 *
 * The resolution of @rin and @iin in the transformed direction has to be from
 * the set of sizes returned by gwy_fft_find_nice_size().  No leveling,
 * windowing nor scaling is performed.
 *
 * Since: 2.1
 **/
void
gwy_data_field_1dfft_raw(GwyDataField *rin,
                         GwyDataField *iin,
                         GwyDataField *rout,
                         GwyDataField *iout,
                         GwyOrientation orientation,
                         GwyTransformDirection direction)
{
    gint newres;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(!iin || GWY_IS_DATA_FIELD(iin));
    if (iin)
        g_return_if_fail(!gwy_data_field_check_compatibility
                                       (rin, iin, GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        newres = gwy_fft_find_nice_size(rin->xres);
        g_return_if_fail(rin->xres == newres);

        gwy_data_field_resample(rout, newres, rin->yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_resample(iout, newres, rin->yres,
                                GWY_INTERPOLATION_NONE);

        if (iin)
            gwy_data_field_xfft_do(rin, iin, rout, iout, direction);
        else {
            iin = gwy_data_field_new_alike(rin, FALSE);
            gwy_data_field_xfft_real_do(rin, iin, rout, iout, direction);
            g_object_unref(iin);
        }
        break;

        case GWY_ORIENTATION_VERTICAL:
        newres = gwy_fft_find_nice_size(rin->yres);
        g_return_if_fail(rin->yres == newres);

        gwy_data_field_resample(rout, rin->xres, newres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_resample(iout, rin->xres, newres,
                                GWY_INTERPOLATION_NONE);

        if (iin)
            gwy_data_field_yfft_do(rin, iin, rout, iout, direction);
        else {
            iin = gwy_data_field_new_alike(rin, FALSE);
            gwy_data_field_yfft_real_do(rin, iin, rout, iout, direction);
            g_object_unref(iin);
        }
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_data_field_area_xfft:
 * @ra: Real input data field.
 * @ia: Imaginary input data field.  It can be %NULL for real-to-complex
 *      transform which can be somewhat faster than complex-to-complex
 *      transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         lines (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all rows in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
static void
gwy_data_field_area_xfft(GwyDataField *rin, GwyDataField *iin,
                         GwyDataField *rout, GwyDataField *iout,
                         gint col, gint row,
                         gint width, gint height,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gint level)
{
    gint k, newxres;
    GwyDataField *rbuf, *ibuf;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iin));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(rin->xres == iin->xres && rin->yres == rout->yres);
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newxres = gwy_fft_find_nice_size(width);

    gwy_data_field_resample(rout, newxres, height, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(iout, newxres, height, GWY_INTERPOLATION_NONE);

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, rbuf->data + k*width, level);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(rbuf, newxres, height, interpolation);

    ibuf = gwy_data_field_area_extract(iin, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, ibuf->data + k*width, level);
    }
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(ibuf, newxres, height, interpolation);

    gwy_data_field_xfft_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        for (k = 0; k < height; k++)
            gwy_preserve_rms_simple(width, 1,
                                    rin->data + rin->xres*(row + k) + col,
                                    iin->data + iin->xres*(row + k) + col,
                                    newxres, 1,
                                    rout->data + newxres*k,
                                    iout->data + newxres*k);
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

static void
gwy_data_field_xfft_do(GwyDataField *rin,
                       GwyDataField *iin,
                       GwyDataField *rout,
                       GwyDataField *iout,
                       GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;

    dims[0].n = rin->xres;
    dims[0].is = 1;
    dims[0].os = 1;
    howmany_dims[0].n = rin->yres;
    howmany_dims[0].is = rin->xres;
    howmany_dims[0].os = rin->xres;
    /* Backward direction is equivalent to switching real and imaginary parts */
    /* XXX: Planner destroys input, we have to either allocate memory or
     * use in-place transform.  In some cases caller could provide us with
     * already allocated buffers. */
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        rout->data, iout->data,
                                        rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    else
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        iout->data, rout->data,
                                        iout->data, rout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    gwy_data_field_copy(rin, rout, FALSE);
    gwy_data_field_copy(iin, iout, FALSE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->xres));
    gwy_data_field_multiply(iout, 1.0/sqrt(rin->xres));
#else
    gint k;

    for (k = 0; k < rin->yres; k++)
        gwy_fft_simple(direction, rin->xres,
                       1, rin->data + k*rin->xres, iin->data + k*rin->xres,
                       1, rout->data + k*rin->xres, iout->data + k*rin->xres);
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_area_xfft_real:
 * @rin: Real input data field.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         lines (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all rows in a data real field with Fast Fourier Transform.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_xfft().
 **/
static void
gwy_data_field_area_xfft_real(GwyDataField *rin, GwyDataField *rout,
                              GwyDataField *iout,
                              gint col, gint row,
                              gint width, gint height,
                              GwyWindowingType windowing,
                              GwyTransformDirection direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms, gint level)
{
    gint newxres, k;
    GwyDataField *rbuf, *ibuf;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newxres = gwy_fft_find_nice_size(width);

    gwy_data_field_resample(rout, newxres, height, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(iout, newxres, height, GWY_INTERPOLATION_NONE);

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, rbuf->data + k*width, level);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(rbuf, newxres, height, interpolation);

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);

    gwy_data_field_xfft_real_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        for (k = 0; k < height; k++)
            gwy_preserve_rms_simple(width, 1,
                                    rin->data + rin->xres*(row + k) + col, NULL,
                                    newxres, 1,
                                    rout->data + newxres*k,
                                    iout->data + newxres*k);
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

static void
gwy_data_field_xfft_real_do(GwyDataField *rin,
                            GwyDataField *ibuf,
                            GwyDataField *rout,
                            GwyDataField *iout,
                            GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;
    gint j, k;

    dims[0].n = rin->xres;
    dims[0].is = 1;
    dims[0].os = 1;
    howmany_dims[0].n = rin->yres;
    howmany_dims[0].is = rin->xres;
    howmany_dims[0].os = rin->xres;
    plan = fftw_plan_guru_split_dft_r2c(1, dims, 1, howmany_dims,
                                        ibuf->data, rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    /* R2C destroys input, and especially, the planner destroys input too */
    gwy_data_field_copy(rin, ibuf, FALSE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    /* Complete the missing half of transform.  */
    for (k = 0; k < rin->yres; k++) {
        gdouble *re, *im;

        re = rout->data + k*rin->xres;
        im = iout->data + k*rin->xres;
        for (j = rin->xres/2 + 1; j < rin->xres; j++) {
            re[j] = re[rin->xres - j];
            im[j] = -im[rin->xres - j];
        }
    }

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->xres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(iout, 1.0/sqrt(rin->xres));
    else
        gwy_data_field_multiply(iout, -1.0/sqrt(rin->xres));
#else
    gint j, k;

    for (k = 0; k+1 < rin->yres; k += 2) {
        gdouble *re, *im, *r0, *r1, *i0, *i1;

        re = ibuf->data + k*rin->xres;
        im = ibuf->data + (k + 1)*rin->xres;
        r0 = rin->data + k*rin->xres;
        r1 = rin->data + (k + 1)*rin->xres;

        gwy_fft_simple(direction, rin->xres, 1, r0, r1, 1, re, im);

        r0 = rout->data + k*rin->xres;
        r1 = rout->data + (k + 1)*rin->xres;
        i0 = iout->data + k*rin->xres;
        i1 = iout->data + (k + 1)*rin->xres;

        /* Disentangle transforms of the row couples */
        r0[0] = re[0];
        i0[0] = 0.0;
        r1[0] = im[0];
        i1[0] = 0.0;
        for (j = 1; j < rin->xres; j++) {
            r0[j] = (re[j] + re[rin->xres - j])/2.0;
            i0[j] = (im[j] - im[rin->xres - j])/2.0;
            r1[j] = (im[j] + im[rin->xres - j])/2.0;
            i1[j] = (-re[j] + re[rin->xres - j])/2.0;
        }
    }
    if (rin->yres % 2) {
        k = rin->xres * (rin->yres - 1);
        memset(ibuf->data, 0, rin->xres*sizeof(gdouble));
        gwy_fft_simple(direction, rin->xres,
                       1, rin->data + k, ibuf->data,
                       1, rout->data + k, iout->data + k);

    }
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_area_yfft:
 * @ra: Real input data field.
 * @ia: Imaginary input data field.  It can be %NULL for real-to-complex
 *      transform which can be somewhat faster than complex-to-complex
 *      transform.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         lines (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all columns in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
static void
gwy_data_field_area_yfft(GwyDataField *rin, GwyDataField *iin,
                         GwyDataField *rout, GwyDataField *iout,
                         gint col, gint row,
                         gint width, gint height,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gint level)
{
    gint k, newyres;
    GwyDataField *rbuf, *ibuf;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iin));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(rin->xres == iin->xres && rin->yres == rout->yres);
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rout, width, newyres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(iout, width, newyres, GWY_INTERPOLATION_NONE);

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, rbuf->data + k, level);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, width, newyres, interpolation);

    ibuf = gwy_data_field_area_extract(iin, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, ibuf->data + k, level);
    }
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(ibuf, width, newyres, interpolation);

    gwy_data_field_yfft_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        for (k = 0; k < width; k++)
            gwy_preserve_rms_simple(height, rin->xres,
                                    rin->data + rin->xres*row + col + k,
                                    iin->data + iin->xres*row + col + k,
                                    newyres, width,
                                    rout->data + k,
                                    iout->data + k);
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

static void
gwy_data_field_yfft_do(GwyDataField *rin,
                       GwyDataField *iin,
                       GwyDataField *rout,
                       GwyDataField *iout,
                       GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;

    dims[0].n = rin->yres;
    dims[0].is = rin->xres;
    dims[0].os = rin->xres;
    howmany_dims[0].n = rin->xres;
    howmany_dims[0].is = 1;
    howmany_dims[0].os = 1;
    /* Backward direction is equivalent to switching real and imaginary parts */
    /* XXX: Planner destroys input, we have to either allocate memory or
     * use in-place transform.  In some cases caller could provide us with
     * already allocated buffers. */
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        rout->data, iout->data,
                                        rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    else
        plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                        iout->data, rout->data,
                                        iout->data, rout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    gwy_data_field_copy(rin, rout, FALSE);
    gwy_data_field_copy(iin, iout, FALSE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->yres));
    gwy_data_field_multiply(iout, 1.0/sqrt(rin->yres));
#else
    gint k;

    for (k = 0; k < rin->xres; k++)
        gwy_fft_simple(direction, rin->yres,
                       rin->xres, rin->data + k, iin->data + k,
                       rin->xres, rout->data + k, iout->data + k);
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

/**
 * gwy_data_field_area_yfft_real:
 * @ra: Real input data field.
 * @rout: Real output data field, it will be resized to area size.
 * @iout: Imaginary output data field, it will be resized to area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns), must be at least 4.
 * @height: Area height (number of rows), must be at least 4.
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: 0 to perform no leveling, 1 to subtract mean value, 2 to subtract
 *         lines (the number can be interpreted as the first polynomial degree
 *         to keep, but only the enumerated three values are available).
 *
 * Transforms all columns in a data real field with Fast Fourier Transform.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_yfft().
 **/
static void
gwy_data_field_area_yfft_real(GwyDataField *rin, GwyDataField *rout,
                              GwyDataField *iout,
                              gint col, gint row,
                              gint width, gint height,
                              GwyWindowingType windowing,
                              GwyTransformDirection direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms, gint level)
{
    gint newyres, k;
    GwyDataField *rbuf, *ibuf;

    g_return_if_fail(GWY_IS_DATA_FIELD(rin));
    g_return_if_fail(GWY_IS_DATA_FIELD(rout));
    g_return_if_fail(GWY_IS_DATA_FIELD(iout));
    g_return_if_fail(level >= 0 && level <= 2);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= rin->xres
                     && row + height <= rin->yres);

    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rout, width, newyres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(iout, width, newyres, GWY_INTERPOLATION_NONE);

    rbuf = gwy_data_field_area_extract(rin, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, rbuf->data + k, level);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, width, newyres, interpolation);

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);

    gwy_data_field_yfft_real_do(rbuf, ibuf, rout, iout, direction);

    if (preserverms) {
        for (k = 0; k < width; k++)
            gwy_preserve_rms_simple(height, rin->xres,
                                    rin->data + rin->xres*row + col + k, NULL,
                                    newyres, width,
                                    rout->data + k,
                                    iout->data + k);
    }

    gwy_data_field_resample(rout, width, height, interpolation);
    gwy_data_field_resample(iout, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
}

static void
gwy_data_field_yfft_real_do(GwyDataField *rin,
                            GwyDataField *ibuf,
                            GwyDataField *rout,
                            GwyDataField *iout,
                            GwyTransformDirection direction)
{
#ifdef HAVE_FFTW3
    fftw_iodim dims[1], howmany_dims[1];
    fftw_plan plan;
    gint j, k;

    dims[0].n = rin->yres;
    dims[0].is = rin->xres;
    dims[0].os = rin->xres;
    howmany_dims[0].n = rin->xres;
    howmany_dims[0].is = 1;
    howmany_dims[0].os = 1;
    plan = fftw_plan_guru_split_dft_r2c(1, dims, 1, howmany_dims,
                                        ibuf->data, rout->data, iout->data,
                                        _GWY_FFTW_PATIENCE);
    g_return_if_fail(plan);
    /* R2C destroys input, and especially, the planner destroys input too */
    gwy_data_field_copy(rin, ibuf, FALSE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    /* Complete the missing half of transform.  */
    for (k = 0; k < rin->xres; k++) {
        gdouble *re, *im;

        re = rout->data + k;
        im = iout->data + k;
        for (j = rin->yres/2 + 1; j < rin->yres; j++) {
            re[rin->xres*j] = re[rin->xres*(rin->yres - j)];
            im[rin->xres*j] = -im[rin->xres*(rin->yres - j)];
        }
    }

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->yres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(iout, 1.0/sqrt(rin->yres));
    else
        gwy_data_field_multiply(iout, -1.0/sqrt(rin->yres));
#else
    gint j, k;

    for (k = 0; k+1 < rin->xres; k += 2) {
        gdouble *re, *im, *r0, *r1, *i0, *i1;

        re = ibuf->data + k;
        im = ibuf->data + (k + 1);
        r0 = rin->data + k;
        r1 = rin->data + (k + 1);

        /* FIXME: we could achieve better data locality by using the in
         * arrays `rotated'. */
        gwy_fft_simple(direction, rin->yres, rin->xres, r0, r1,
                       rin->xres, re, im);

        r0 = rout->data + k;
        r1 = rout->data + (k + 1);
        i0 = iout->data + k;
        i1 = iout->data + (k + 1);

        /* Disentangle transforms of the row couples */
        r0[0] = re[0];
        i0[0] = 0.0;
        r1[0] = im[0];
        i1[0] = 0.0;
        for (j = 1; j < rin->yres; j++) {
            gint n = rin->xres*rin->yres, kj = rin->xres*j;

            r0[rin->xres*j] = (re[kj] + re[n - kj])/2.0;
            i0[rin->xres*j] = (im[kj] - im[n - kj])/2.0;
            r1[rin->xres*j] = (im[kj] + im[n - kj])/2.0;
            i1[rin->xres*j] = (-re[kj] + re[n - kj])/2.0;
        }
    }
    if (rin->xres % 2) {
        k = rin->xres - 1;
        for (j = 0; j < rin->yres; j++)
            ibuf->data[j*rin->xres + k] = 0.0;
        gwy_fft_simple(direction, rin->yres,
                       rin->xres, rin->data + k, ibuf->data + k,
                       rin->xres, rout->data + k, iout->data + k);

    }
#endif
    gwy_data_field_invalidate(rout);
    gwy_data_field_invalidate(iout);
}

static void
gwy_level_simple(gint n,
                 gint stride,
                 gdouble *data,
                 gint level)
{
    gdouble sumxi, sumxixi, sumsi, sumsixi, a, b;
    gdouble *pdata;
    gint i;

    if (!level)
        return;

    if (level == 1) {
        sumsi = 0.0;
        pdata = data;
        for (i = n; i; i--, pdata += stride)
            sumsi += *pdata;

        a = sumsi/n;
        pdata = data;
        for (i = n; i; i--, pdata += stride)
            *pdata -= a;

        return;
    }

    g_return_if_fail(level == 2);

    /* These are already averages, not sums */
    sumxi = (n + 1.0)/2.0;
    sumxixi = (2.0*n + 1.0)*(n + 1.0)/6.0;

    sumsi = sumsixi = 0.0;

    pdata = data;
    for (i = n; i; i--, pdata += stride) {
        sumsi += *pdata;
        sumsixi += *pdata * i;
    }
    sumsi /= n;
    sumsixi /= n;

    b = (sumsixi - sumsi*sumxi)/(sumxixi - sumxi*sumxi);
    a = (sumsi*sumxixi - sumxi*sumsixi)/(sumxixi - sumxi*sumxi);

    pdata = data;
    sumsi = 0;
    for (i = n; i; i--, pdata += stride) {
        *pdata -= a + b*i;
        sumsi += *pdata;
    }
}

static void
gwy_preserve_rms_simple(gint nsrc,
                        gint stridesrc,
                        const gdouble *src1,
                        const gdouble *src2,
                        gint ndata,
                        gint stridedata,
                        gdouble *data1,
                        gdouble *data2)
{
    gdouble sum2, sum0, sum02, a, b, q;
    gdouble *pdata;
    gint i;

    /* Calculate original RMS */
    sum0 = sum02 = 0.0;
    for (i = nsrc; i; i--, src1 += stridesrc) {
        sum0 += *src1;
        sum02 += *src1 * *src1;
    }
    a = sum02 - sum0*sum0/nsrc;
    if (src2) {
        sum0 = sum02 = 0.0;
        for (i = nsrc; i; i--, src1 += stridesrc) {
            sum0 += *src2;
            sum02 += *src2 * *src2;
        }
        a += sum02 - sum0*sum0/nsrc;
    }
    if (a <= 0.0)
        return;
    a = sqrt(a/nsrc);

    /* Calculare new RMS ignoring 0th elements that correspond to constants */
    sum2 = 0.0;
    for (i = ndata-1, pdata = data1 + 1; i; i--, pdata += stridedata)
        sum2 += *pdata * *pdata;
    for (i = ndata-1, pdata = data2 + 1; i; i--, pdata += stridedata)
        sum2 += *pdata * *pdata;
    if (sum2 == 0.0)
        return;
    b = sqrt(sum2/ndata);

    /* Multiply output to get the same RMS */
    q = a/b;
    for (i = ndata, pdata = data1; i; i--, pdata += stridedata)
        *pdata *= q;
    for (i = ndata, pdata = data2; i; i--, pdata += stridedata)
        *pdata *= q;
}

/**
 * gwy_data_field_mult_wav:
 * @real_field: A data field of real values
 * @imag_field: A data field of imaginary values
 * @scale: wavelet scale
 * @wtype: waveelt type
 *
 * multiply a complex data field (real and imaginary)
 * with complex FT of spoecified wavelet at given scale.
 **/
static void
gwy_data_field_mult_wav(GwyDataField *real_field,
                        GwyDataField *imag_field,
                        gdouble scale,
                        Gwy2DCWTWaveletType wtype)
{
    gint xres, yres, xresh, yresh;
    gint i, j;
    gdouble mval, val;

    xres = real_field->xres;
    yres = real_field->yres;
    xresh = xres/2;
    yresh = yres/2;

    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            val = 1;
            if (i < xresh) {
                if (j < yresh)
                    mval = hypot(i, j);
                else
                    mval = hypot(i, yres - j);
            }
            else {
                if (j < yresh)
                    mval = hypot(xres - i, j);
                else
                    mval = hypot(xres - i, yres - j);
            }
            val = gwy_cwt_wfunc_2d(scale, mval, xres, wtype);

            real_field->data[i + j * xres] *= val;
            imag_field->data[i + j * xres] *= val;
        }
    }
}

/**
 * gwy_data_field_cwt:
 * @data_field: A data field.
 * @interpolation: Interpolation type.
 * @scale: Wavelet scale.
 * @wtype: Wavelet type.
 *
 * Computes a continuous wavelet transform (CWT) at given
 * scale and using given wavelet.
 **/
void
gwy_data_field_cwt(GwyDataField *data_field,
                   GwyInterpolationType interpolation,
                   gdouble scale,
                   Gwy2DCWTWaveletType wtype)
{
    GwyDataField *hlp_r, *hlp_i, *imag_field;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    hlp_r = gwy_data_field_new_alike(data_field, FALSE);
    hlp_i = gwy_data_field_new_alike(data_field, FALSE);
    imag_field = gwy_data_field_new_alike(data_field, TRUE);

    gwy_data_field_2dfft(data_field,
                         imag_field,
                         hlp_r,
                         hlp_i,
                         GWY_WINDOWING_RECT,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         interpolation,
                         FALSE,
                         FALSE);
    gwy_data_field_mult_wav(hlp_r, hlp_i, scale, wtype);

    gwy_data_field_2dfft(hlp_r,
                         hlp_i,
                         data_field,
                         imag_field,
                         GWY_WINDOWING_RECT,
                         GWY_TRANSFORM_DIRECTION_BACKWARD,
                         interpolation,
                         FALSE,
                         FALSE);

    g_object_unref(hlp_r);
    g_object_unref(hlp_i);
    g_object_unref(imag_field);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_fft_filter_1d:
 * @data_field: A data field to filter.
 * @result_field: A data field to store the result to.  It will be resampled
 *                to @data_field's size.
 * @weights: Filter weights for the lower half of the spectrum (the other
 *           half is symmetric).  Its size can be arbitrary, it will be
 *           interpolated.
 * @orientation: Filter direction.
 * @interpolation: The interpolation to use for resampling.
 *
 * Performs 1D FFT filtering of a data field.
 **/
void
gwy_data_field_fft_filter_1d(GwyDataField *data_field,
                             GwyDataField *result_field,
                             GwyDataLine *weights,
                             GwyOrientation orientation,
                             GwyInterpolationType interpolation)
{
    GwyDataField *buffer, *iresult_field, *hlp_rdfield, *hlp_idfield;
    GwyDataLine *w;
    gint i, j, size, yres;

    buffer = gwy_data_field_new_alike(data_field, FALSE);
    gwy_data_field_copy(data_field, buffer, FALSE);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(buffer, G_PI/2, interpolation);

    yres = buffer->yres;
    size = gwy_fft_find_nice_size(gwy_data_field_get_xres(buffer));
    gwy_data_field_resample(buffer, size, yres, interpolation);
    gwy_data_field_resample(result_field, size, yres, GWY_INTERPOLATION_NONE);

    hlp_rdfield = gwy_data_field_new_alike(buffer, TRUE);
    hlp_idfield = gwy_data_field_new_alike(buffer, TRUE);
    iresult_field = gwy_data_field_new_alike(buffer, TRUE);

    gwy_data_field_1dfft_raw(buffer, NULL,
                             hlp_rdfield, hlp_idfield,
                             GWY_ORIENTATION_HORIZONTAL,
                             GWY_TRANSFORM_DIRECTION_FORWARD);

    w = gwy_data_line_new_resampled(weights, (size + 1)/2, interpolation);
    for (i = 0; i < yres; i++) {
        gdouble *rrow = hlp_rdfield->data + i*size;
        gdouble *irow = hlp_idfield->data + i*size;

        for (j = 0; j < size/2; j++) {
            rrow[j] *= w->data[j];
            rrow[size-1 - j] *= w->data[j];
            irow[j] *= w->data[j];
            irow[size-1 - j] *= w->data[j];
        }
        if (w->res != size/2) {
            rrow[size/2] *= w->data[size/2];
            irow[size/2] *= w->data[size/2];
        }
    }
    g_object_unref(w);

    gwy_data_field_1dfft_raw(hlp_rdfield, hlp_idfield,
                             result_field, iresult_field,
                             GWY_ORIENTATION_HORIZONTAL,
                             GWY_TRANSFORM_DIRECTION_BACKWARD);

    gwy_data_field_resample(result_field, data_field->xres, data_field->yres,
                            interpolation);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(result_field, -G_PI/2, interpolation);

    g_object_unref(hlp_rdfield);
    g_object_unref(hlp_idfield);
    g_object_unref(iresult_field);
    g_object_unref(buffer);
}

/************************** Documentation ****************************/

/**
 * SECTION:inttrans
 * @title: inttrans
 * @short_description: FFT and other integral transforms
 *
 * There are two main groups of FFT functions.
 *
 * High-level functions such as gwy_data_field_2dfft(), gwy_data_line_fft()
 * can perform windowing, leveling and other pre- and postprocessing.  They
 * also automatically resample data to a size supported by the current FFT
 * backend.  This makes them suitable for calculation of spectral densities
 * and other statistical characteristics.
 *
 * Low-level functions have <literal>raw</literal> appended to their name:
 * gwy_data_field_2dfft_raw(), gwy_data_line_fft_raw().  They expect data
 * of a size returned by gwy_fft_find_nice_size() and perform no other
 * operations on the data beside the transform itself.  This makes them
 * suitable for applications where both forward and inverse transform is
 * performed.
 *
 * Both types of functions wrap a Fourier transform backend which can be
 * currently either gwy_fft_simple(), available always, or
 * <ulink url="http://fftw.org/">FFTW3</ulink>, available when Gwyddion was
 * compiled with it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
