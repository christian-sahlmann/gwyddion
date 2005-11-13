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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/linestats.h>
#include <libprocess/simplefft.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/cwt.h>

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
                                            gboolean level);
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
                                            gboolean level);
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
                                            gboolean level);
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
                                            gboolean level);
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
                                            gboolean level);
static void  gwy_data_field_mult_wav       (GwyDataField *real_field,
                                            GwyDataField *imag_field,
                                            gdouble scale,
                                            Gwy2DCWTWaveletType wtype);
static void  gwy_level_simple              (gint n,
                                            gint stride,
                                            gdouble *data);
static gdouble edist                       (gint xc1,
                                            gint yc1,
                                            gint xc2,
                                            gint yc2);

#ifdef HAVE_FFTW3
/* Good FFTW array sizes for extended and resampled arrays.
 *
 * Since extending and resampling always involves an O(N) part -- N being the
 * extended array size -- that may even be dominant, it isn't wise to use the
 * fastest possible FFT if it requires considerably larger array.  Following
 * numbers represent a reasonable compromise tested on a few platforms */
static const guint16 nice_fftw_num[] = {
       18,    20,    22,    24,    25,    27,    28,    30,    32,    33,
       35,    36,    40,    42,    44,    72,    75,    77,    81,    84,
       88,    90,    96,    98,    99,   100,   105,   108,   110,   112,
      120,   126,   128,   132,   135,   140,   144,   150,   154,   160,
      165,   168,   175,   176,   180,   189,   196,   198,   200,   210,
      216,   220,   224,   225,   231,   240,   243,   250,   252,   256,
      264,   270,   275,   280,   288,   294,   297,   300,   308,   315,
      320,   324,   330,   336,   343,   350,   352,   360,   375,   384,
      385,   396,   400,   405,   420,   432,   441,   448,   450,   462,
      480,   486,   490,   495,   500,   504,   512,   528,   540,   550,
      576,   588,   594,   600,   616,   630,   640,   648,   672,   675,
      686,   700,   704,   720,   729,   735,   750,   768,   770,   784,
      792,   800,   810,   825,   840,   864,   896,   900,   924,   960,
      972,   980,   990,  1000,  1008,  1024,  1050,  1056,  1080,  1100,
     1120,  1152,  1155,  1176,  1200,  1215,  1232,  1280,  1296,  1320,
     1344,  1350,  1400,  1408,  1440,  1470,  1536,  1568,  1575,  1584,
     1620,  1680,  1728,  1760,  1792,  1800,  1920,  2048,  2058,  2100,
     2112,  2160,  2240,  2250,  2304,  2310,  2352,  2400,  2430,  2464,
     2520,  2560,  2592,  2688,  2700,  2744,  2800,  2816,  2880,  3072,
     3136,  3200,  3240,  3360,  3456,  3520,  3584,  3600,  3645,  3840,
     4096,  4116,  4200,  4224,  4320,  4480,  4608,  4704,  4800,  4860,
     5120,  5184,  5376,  5400,  5488,  5632,  5760,  6144,  6272,  6400,
     6480,  6720,  6750,  6912,  7168,  7200,  7680,  7776,  7840,  8000,
     8064,  8192,  8232,  8400,  8448,  8640,  8960,  9216,  9408,  9600,
     9720,  9800, 10080, 10240, 10368, 10560, 10752, 10800, 10976, 11088,
    11520, 11760, 12288, 12544, 12600, 12800, 12960, 13200, 13440, 13824,
    14080, 14112, 14336, 14400, 15360, 15552, 15680, 16000, 16128, 16200,
    16384, 16464, 16632, 16800, 16875, 17280, 17600, 17920, 18432, 18480,
    19200, 19440, 19600, 19712, 19800, 20160, 20250, 20480, 20736, 21120,
    21504, 21600, 21870, 22050, 22176, 22400, 23040, 23100, 23328, 23520,
    23760, 24000, 24192, 24300, 24576, 24640, 24696, 24750, 25088, 25200,
    25344, 25600, 25920, 26400, 26880, 27000, 27648, 27720, 28160, 28672,
    28800, 29160, 29400, 29568, 30000, 30720, 31104, 31680, 32000, 32256,
    32400, 33000, 33264, 33600, 33792, 34560, 34992, 35000, 35200, 35280,
    35640, 35840, 36000, 36450, 36864, 36960, 37632, 37800, 38016, 38400,
    38880, 39424, 39600, 40000, 40320, 40500, 40960, 41472, 42000, 42240,
    42336, 42525, 43008, 43200, 43218, 43740, 43904, 44000, 44352, 44550,
    44800, 45000, 45056, 45360, 46080, 46656, 47520, 48000, 48384, 48600,
    49152, 49280, 49392, 49500, 50176, 50400, 50625, 50688, 51840, 52800,
    52920, 53760, 54000, 54432, 54675, 55296, 55440, 56250, 57600, 58320,
    58800, 59400, 60000, 60480, 60750, 61440, 61740, 62208, 63000, 63360,
    64800,
};

/* Indices of powers of 2 in nice_fftw_numbers[], starting from 2^4 */
static const guint nice_fftw_num_2n[] = {
    0, 8, 15, 32, 59, 96, 135, 167, 200, 231, 270, 331
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
    gint x, p2;

    if (size <= 1 << 4)
        return size;
    g_return_val_if_fail(size <= nice_fftw_num[G_N_ELEMENTS(nice_fftw_num)-1],
                         size);
    for (x = size >> 4, p2 = 0; x; p2++, x = x >> 1)
        ;
    for (x = nice_fftw_num_2n[p2-1]; nice_fftw_num[x] < size; x++)
        ;
    return nice_fftw_num[x];
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
 * @rdest: Real output data line.
 * @idest: Imaginary output data line.
 * @windowing: Windowing mode.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS value while windowing.
 * @level: %TRUE to level line before computation.
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
                  gboolean level)
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
 * @isrc: Imaginary input data line.
 * @rdest: Real output data line, it will be resized to @len.
 * @idest: Imaginary output data line, it will be resized to @len.
 * @from: The index in input lines to start from (inclusive).
 * @len: Lenght of data line part, it must be at least 4.
 * @windowing: Windowing mode.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS value while windowing.
 * @level: %TRUE to level line before computation.
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
                       gboolean level)
{
    gint newres, i;
    GwyDataLine *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;
    gdouble rmsa = 0.0, rmsb;

    g_return_if_fail(GWY_IS_DATA_LINE(rsrc));
    g_return_if_fail(GWY_IS_DATA_LINE(isrc));
    g_return_if_fail(GWY_IS_DATA_LINE(rdest));
    g_return_if_fail(GWY_IS_DATA_LINE(idest));
    g_return_if_fail(rsrc->res == isrc->res);
    g_return_if_fail(from >= 0
                     && len >= 4
                     && from + len <= rsrc->res);

    newres = gwy_fft_find_nice_size(rsrc->res);

    gwy_data_line_resample(rdest, newres, GWY_INTERPOLATION_NONE);
    out_rdata = rdest->data;

    gwy_data_line_resample(idest, newres, GWY_INTERPOLATION_NONE);
    out_idata = idest->data;

    rbuf = gwy_data_line_part_extract(rsrc, from, len);
    if (level)
        gwy_level_simple(len, 1, rbuf->data);
    if (preserverms)
        rmsa = gwy_data_line_get_rms(rbuf);
    gwy_fft_window(len, rbuf->data, windowing);
    gwy_data_line_resample(rbuf, newres, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_line_part_extract(isrc, from, len);
    if (level)
        gwy_level_simple(len, 1, ibuf->data);
    if (preserverms)
        rmsa = hypot(rmsa, gwy_data_line_get_rms(ibuf));
    gwy_fft_window(len, ibuf->data, windowing);
    gwy_data_line_resample(ibuf, newres, interpolation);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[1], howmany_dims[1];
        fftw_plan plan;

        dims[0].n = newres;
        dims[0].is = 1;
        dims[0].os = 1;
        howmany_dims[0].n = 1;
        howmany_dims[0].is = newres;
        howmany_dims[0].os = newres;
        if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_rdata, in_idata,
                                            out_rdata, out_idata,
                                            FFTW_MEASURE);
        else
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_idata, in_rdata,
                                            out_idata, out_rdata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    }
    gwy_data_line_multiply(rdest, 1.0/sqrt(newres));
    gwy_data_line_multiply(idest, 1.0/sqrt(newres));
#else
    gwy_fft_simple(direction, newres,
                   1, in_rdata, in_idata,
                   1, out_rdata, out_idata);
#endif

    if (preserverms) {
        rmsb = 0.0;
        for (i = 1; i < newres; i++)
            rmsb += out_rdata[i]*out_rdata[i] + out_idata[i]*out_idata[i];
        rmsb = sqrt(rmsb)/newres;
        if (rmsb > 0.0) {
            gwy_data_line_multiply(rdest, rmsa/rmsb);
            gwy_data_line_multiply(idest, rmsa/rmsb);
        }
    }

    gwy_data_line_resample(rdest, len, interpolation);
    gwy_data_line_resample(idest, len, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);
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
 * @level: %TRUE to level data before computation.
 *
 * Calculates 2D Fast Fourier Transform of a rectangular area of a data field.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_area_2dfft(GwyDataField *ra, GwyDataField *ia,
                          GwyDataField *rb, GwyDataField *ib,
                          gint col, gint row,
                          gint width, gint height,
                          GwyWindowingType windowing,
                          GwyTransformDirection direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms, gboolean level)
{
    gint j, k, newxres, newyres;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;
    gdouble a, bx, by, rmsa = 0.0, rmsb;

    if (!ia) {
        gwy_data_field_area_2dfft_real(ra, rb, ib,
                                       col, row, width, height,
                                       windowing, direction, interpolation,
                                       preserverms, level);
        return;
    }

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ia));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(ra->xres == ia->xres && ra->yres == rb->yres);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newxres = gwy_fft_find_nice_size(width);
    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rb, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        gwy_data_field_fit_plane(rbuf, &a,  &bx, &by);
        gwy_data_field_plane_level(rbuf, a, bx, by);
    }
    if (preserverms)
        rmsa = gwy_data_field_get_rms(rbuf);
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, newxres, newyres, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_area_extract(ia, col, row, width, height);
    if (level) {
        gwy_data_field_fit_plane(ibuf, &a,  &bx, &by);
        gwy_data_field_plane_level(ibuf, a, bx, by);
    }
    if (preserverms)
        rmsa = hypot(rmsa, gwy_data_field_get_rms(ibuf));
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(ibuf, newxres, newyres, interpolation);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[2], howmany_dims[1];
        fftw_plan plan;

        dims[1].n = newxres;
        dims[1].is = 1;
        dims[1].os = 1;
        dims[0].n = newyres;
        dims[0].is = dims[1].is * dims[1].n;
        dims[0].os = dims[1].os * dims[1].n;
        howmany_dims[0].n = 1;
        howmany_dims[0].is = newxres*newyres;
        howmany_dims[0].os = newxres*newyres;
        if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
            plan = fftw_plan_guru_split_dft(2, dims, 1, howmany_dims,
                                            in_rdata, in_idata,
                                            out_rdata, out_idata,
                                            FFTW_MEASURE);
        else
            plan = fftw_plan_guru_split_dft(2, dims, 1, howmany_dims,
                                            in_idata, in_rdata,
                                            out_idata, out_rdata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newxres*newyres));
    gwy_data_field_multiply(ib, 1.0/sqrt(newxres*newyres));
#else
    {
        for (k = 0; k < newyres; k++) {
            gwy_fft_simple(direction, newxres,
                           1, in_rdata + k*newxres, in_idata + k*newxres,
                           1, out_rdata + k*newxres, out_idata + k*newxres);
        }
        /* FIXME: this is a bit cruel */
        gwy_data_field_copy(rb, rbuf, FALSE);
        gwy_data_field_copy(ib, ibuf, FALSE);
        for (k = 0; k < newxres; k++) {
            gwy_fft_simple(direction, newyres,
                           newxres, in_rdata + k, in_idata + k,
                           newxres, out_rdata + k, out_idata + k);
        }
    }
#endif

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
            gwy_data_field_multiply(rb, rmsa/rmsb);
            gwy_data_field_multiply(ib, rmsa/rmsb);
        }
    }

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_area_2dfft_real:
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
 * @level: %TRUE to level data before computation.
 *
 * Calculates 2D Fast Fourier Transform of a rectangular area of a data field.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_2dfft().
 **/
static void
gwy_data_field_area_2dfft_real(GwyDataField *ra,
                               GwyDataField *rb, GwyDataField *ib,
                               gint col, gint row,
                               gint width, gint height,
                               GwyWindowingType windowing,
                               GwyTransformDirection direction,
                               GwyInterpolationType interpolation,
                               gboolean preserverms, gboolean level)
{
    gint newxres, newyres, j, k;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;
    gdouble a, bx, by, rmsa = 0.0, rmsb;

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newxres = gwy_fft_find_nice_size(width);
    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rb, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, newxres, newyres, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        gwy_data_field_fit_plane(rbuf, &a,  &bx, &by);
        gwy_data_field_plane_level(rbuf, a, bx, by);
    }
    if (preserverms)
        rmsa = gwy_data_field_get_rms(rbuf);
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, newxres, newyres, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[2], howmany_dims[1];
        fftw_plan plan;

        dims[1].n = newxres;
        dims[1].is = 1;
        dims[1].os = 1;
        dims[0].n = newyres;
        dims[0].is = dims[1].is * dims[1].n;
        dims[0].os = dims[1].os * dims[1].n;
        howmany_dims[0].n = 1;
        howmany_dims[0].is = newxres*newyres;
        howmany_dims[0].os = newxres*newyres;
        plan = fftw_plan_guru_split_dft_r2c(2, dims, 1, howmany_dims,
                                            in_idata, out_rdata, out_idata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        /* R2C destroys input, and especially, the planner destroys input too */
        gwy_data_field_copy(rbuf, ibuf, FALSE);
        fftw_execute(plan);
        fftw_destroy_plan(plan);

        /* Complete the missing half of transform.  */
        for (j = newxres/2 + 1; j < newxres; j++) {
            out_rdata[j] = out_rdata[newxres - j];
            out_idata[j] = -out_idata[newxres - j];
        }
        for (k = 1; k < newyres; k++) {
            gdouble *r0, *i0, *r1, *i1;

            r0 = out_rdata + k*newxres;
            i0 = out_idata + k*newxres;
            r1 = out_rdata + (newyres - k)*newxres;
            i1 = out_idata + (newyres - k)*newxres;
            for (j = newxres/2 + 1; j < newxres; j++) {
                r0[j] = r1[newxres - j];
                i0[j] = -i1[newxres - j];
            }
        }
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newxres*newyres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(ib, 1.0/sqrt(newxres*newyres));
    else
        gwy_data_field_multiply(ib, -1.0/sqrt(newxres*newyres));
#else
    {
        for (k = 0; k < newyres; k += 2) {
            gdouble *re, *im, *r0, *r1, *i0, *i1;

            re = out_rdata + k*newxres;
            im = out_rdata + (k + 1)*newxres;
            r0 = in_rdata + k*newxres;
            r1 = in_rdata + (k + 1)*newxres;
            i0 = in_idata + k*newxres;
            i1 = in_idata + (k + 1)*newxres;

            gwy_fft_simple(direction, newxres, 1, r0, r1, 1, re, im);

            /* Disentangle transforms of the row couples */
            r0[0] = re[0];
            i0[0] = 0.0;
            r1[0] = im[0];
            i1[0] = 0.0;
            for (j = 1; j < newxres; j++) {
                r0[j] = (re[j] + re[newxres - j])/2.0;
                i0[j] = (im[j] - im[newxres - j])/2.0;
                r1[j] = (im[j] + im[newxres - j])/2.0;
                i1[j] = (-re[j] + re[newxres - j])/2.0;
            }
        }
        for (k = 0; k < newxres; k++) {
            gwy_fft_simple(direction, newyres,
                           newxres, in_rdata + k, in_idata + k,
                           newxres, out_rdata + k, out_idata + k);
        }
    }
#endif

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
            gwy_data_field_multiply(rb, rmsa/rmsb);
            gwy_data_field_multiply(ib, rmsa/rmsb);
        }
    }

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
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
 * @level: %TRUE to level data before computation.
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
                     gboolean preserverms, gboolean level)
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
 * @level: %TRUE to level data before computation.
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
                          gboolean level)
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
 * @level: %TRUE to level data before computation.
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
                     gboolean level)
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
 * @level: %TRUE to level data before computation.
 *
 * Transforms all rows in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
static void
gwy_data_field_area_xfft(GwyDataField *ra, GwyDataField *ia,
                         GwyDataField *rb, GwyDataField *ib,
                         gint col, gint row,
                         gint width, gint height,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gboolean level)
{
    gint k, newxres;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ia));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(ra->xres == ia->xres && ra->yres == rb->yres);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newxres = gwy_fft_find_nice_size(width);

    gwy_data_field_resample(rb, newxres, height, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, newxres, height, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, rbuf->data + k*width);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(rbuf, newxres, height, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_area_extract(ia, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, ibuf->data + k*width);
    }
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(ibuf, newxres, height, interpolation);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[1], howmany_dims[1];
        fftw_plan plan;

        dims[0].n = newxres;
        dims[0].is = 1;
        dims[0].os = 1;
        howmany_dims[0].n = height;
        howmany_dims[0].is = newxres;
        howmany_dims[0].os = newxres;
        if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_rdata, in_idata,
                                            out_rdata, out_idata,
                                            FFTW_MEASURE);
        else
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_idata, in_rdata,
                                            out_idata, out_rdata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newxres));
    gwy_data_field_multiply(ib, 1.0/sqrt(newxres));
#else
    {
        gint k;

        for (k = 0; k < height; k++) {
            gwy_fft_simple(direction, newxres,
                           1, in_rdata + k*newxres, in_idata + k*newxres,
                           1, out_rdata + k*newxres, out_idata + k*newxres);
        }
    }
#endif

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_area_xfft_real:
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
 * @level: %TRUE to level data before computation.
 *
 * Transforms all rows in a data real field with Fast Fourier Transform.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_xfft().
 **/
static void
gwy_data_field_area_xfft_real(GwyDataField *ra, GwyDataField *rb,
                              GwyDataField *ib,
                              gint col, gint row,
                              gint width, gint height,
                              GwyWindowingType windowing,
                              GwyTransformDirection direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms, gboolean level)
{
    gint newxres, j, k;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newxres = gwy_fft_find_nice_size(width);

    gwy_data_field_resample(rb, newxres, height, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, newxres, height, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        for (k = 0; k < height; k++)
            gwy_level_simple(width, 1, rbuf->data + k*width);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_HORIZONTAL, windowing);
    gwy_data_field_resample(rbuf, newxres, height, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[1], howmany_dims[1];
        fftw_plan plan;

        dims[0].n = newxres;
        dims[0].is = 1;
        dims[0].os = 1;
        howmany_dims[0].n = height;
        howmany_dims[0].is = newxres;
        howmany_dims[0].os = newxres;
        plan = fftw_plan_guru_split_dft_r2c(1, dims, 1, howmany_dims,
                                            in_idata, out_rdata, out_idata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        /* R2C destroys input, and especially, the planner destroys input too */
        gwy_data_field_copy(rbuf, ibuf, FALSE);
        fftw_execute(plan);
        fftw_destroy_plan(plan);

        /* Complete the missing half of transform.  */
        for (k = 0; k < height; k++) {
            gdouble *re, *im;

            re = out_rdata + k*newxres;
            im = out_idata + k*newxres;
            for (j = newxres/2 + 1; j < newxres; j++) {
                re[j] = re[newxres - j];
                im[j] = -im[newxres - j];
            }
        }
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newxres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(ib, 1.0/sqrt(newxres));
    else
        gwy_data_field_multiply(ib, -1.0/sqrt(newxres));
#else
    {
        for (k = 0; k < height; k += 2) {
            gdouble *re, *im, *r0, *r1, *i0, *i1;

            re = in_idata + k*newxres;
            im = in_idata + (k + 1)*newxres;
            r0 = in_rdata + k*newxres;
            r1 = in_rdata + (k + 1)*newxres;

            gwy_fft_simple(direction, newxres, 1, r0, r1, 1, re, im);

            r0 = out_rdata + k*newxres;
            r1 = out_rdata + (k + 1)*newxres;
            i0 = out_idata + k*newxres;
            i1 = out_idata + (k + 1)*newxres;

            /* Disentangle transforms of the row couples */
            r0[0] = re[0];
            i0[0] = 0.0;
            r1[0] = im[0];
            i1[0] = 0.0;
            for (j = 1; j < newxres; j++) {
                r0[j] = (re[j] + re[newxres - j])/2.0;
                i0[j] = (im[j] - im[newxres - j])/2.0;
                r1[j] = (im[j] + im[newxres - j])/2.0;
                i1[j] = (-re[j] + re[newxres - j])/2.0;
            }
        }
    }
#endif

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
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
 * @level: %TRUE to level data before computation.
 *
 * Transforms all columns in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
static void
gwy_data_field_area_yfft(GwyDataField *ra, GwyDataField *ia,
                         GwyDataField *rb, GwyDataField *ib,
                         gint col, gint row,
                         gint width, gint height,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gboolean level)
{
    gint k, newyres;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ia));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(ra->xres == ia->xres && ra->yres == rb->yres);
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rb, width, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, width, newyres, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, rbuf->data + k);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, width, newyres, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_area_extract(ia, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, ibuf->data + k);
    }
    gwy_fft_window_data_field(ibuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(ibuf, width, newyres, interpolation);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[1], howmany_dims[1];
        fftw_plan plan;

        dims[0].n = newyres;
        dims[0].is = width;
        dims[0].os = width;
        howmany_dims[0].n = width;
        howmany_dims[0].is = 1;
        howmany_dims[0].os = 1;
        if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_rdata, in_idata,
                                            out_rdata, out_idata,
                                            FFTW_MEASURE);
        else
            plan = fftw_plan_guru_split_dft(1, dims, 1, howmany_dims,
                                            in_idata, in_rdata,
                                            out_idata, out_rdata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newyres));
    gwy_data_field_multiply(ib, 1.0/sqrt(newyres));
#else
    {
        for (k = 0; k < width; k++) {
            gwy_fft_simple(direction, newyres,
                           width, in_rdata + k, in_idata + k,
                           width, out_rdata + k, out_idata + k);
        }
    }
#endif

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
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
 * @level: %TRUE to level data before computation.
 *
 * Transforms all columns in a data real field with Fast Fourier Transform.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_yfft().
 **/
static void
gwy_data_field_area_yfft_real(GwyDataField *ra, GwyDataField *rb,
                              GwyDataField *ib,
                              gint col, gint row,
                              gint width, gint height,
                              GwyWindowingType windowing,
                              GwyTransformDirection direction,
                              GwyInterpolationType interpolation,
                              gboolean preserverms, gboolean level)
{
    gint newyres, j, k;
    GwyDataField *rbuf, *ibuf;
    gdouble *in_rdata, *in_idata, *out_rdata, *out_idata;

    g_return_if_fail(GWY_IS_DATA_FIELD(ra));
    g_return_if_fail(GWY_IS_DATA_FIELD(rb));
    g_return_if_fail(GWY_IS_DATA_FIELD(ib));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 4 && height >= 4
                     && col + width <= ra->xres
                     && row + height <= ra->yres);

    newyres = gwy_fft_find_nice_size(height);

    gwy_data_field_resample(rb, width, newyres, GWY_INTERPOLATION_NONE);
    out_rdata = rb->data;

    gwy_data_field_resample(ib, width, newyres, GWY_INTERPOLATION_NONE);
    out_idata = ib->data;

    rbuf = gwy_data_field_area_extract(ra, col, row, width, height);
    if (level) {
        for (k = 0; k < width; k++)
            gwy_level_simple(height, width, rbuf->data + k);
    }
    gwy_fft_window_data_field(rbuf, GWY_ORIENTATION_VERTICAL, windowing);
    gwy_data_field_resample(rbuf, width, newyres, interpolation);
    in_rdata = rbuf->data;

    ibuf = gwy_data_field_new_alike(rbuf, FALSE);
    in_idata = ibuf->data;

#ifdef HAVE_FFTW3
    {
        fftw_iodim dims[1], howmany_dims[1];
        fftw_plan plan;

        dims[0].n = newyres;
        dims[0].is = width;
        dims[0].os = width;
        howmany_dims[0].n = width;
        howmany_dims[0].is = 1;
        howmany_dims[0].os = 1;
        plan = fftw_plan_guru_split_dft_r2c(1, dims, 1, howmany_dims,
                                            in_idata, out_rdata, out_idata,
                                            FFTW_MEASURE);
        g_return_if_fail(plan);
        /* R2C destroys input, and especially, the planner destroys input too */
        gwy_data_field_copy(rbuf, ibuf, FALSE);
        fftw_execute(plan);
        fftw_destroy_plan(plan);

        /* Complete the missing half of transform.  */
        for (k = 0; k < width; k++) {
            gdouble *re, *im;

            re = out_rdata + k;
            im = out_idata + k;
            for (j = newyres/2 + 1; j < newyres; j++) {
                re[width*j] = re[width*(newyres - j)];
                im[width*j] = -im[width*(newyres - j)];
            }
        }
    }
    gwy_data_field_multiply(rb, 1.0/sqrt(newyres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(ib, 1.0/sqrt(newyres));
    else
        gwy_data_field_multiply(ib, -1.0/sqrt(newyres));
#else
    {
        for (k = 0; k < width; k += 2) {
            gdouble *re, *im, *r0, *r1, *i0, *i1;

            re = in_idata + k;
            im = in_idata + (k + 1);
            r0 = in_rdata + k;
            r1 = in_rdata + (k + 1);

            /* FIXME: we could achieve better data locality by using the in
             * arrays `rotated'. */
            gwy_fft_simple(direction, newyres, width, r0, r1, width, re, im);

            r0 = out_rdata + k;
            r1 = out_rdata + (k + 1);
            i0 = out_idata + k;
            i1 = out_idata + (k + 1);

            /* Disentangle transforms of the row couples */
            r0[0] = re[0];
            i0[0] = 0.0;
            r1[0] = im[0];
            i1[0] = 0.0;
            for (j = 1; j < newyres; j++) {
                r0[width*j] = (re[width*j] + re[width*(newyres - j)])/2.0;
                i0[width*j] = (im[width*j] - im[width*(newyres - j)])/2.0;
                r1[width*j] = (im[width*j] + im[width*(newyres - j)])/2.0;
                i1[width*j] = (-re[width*j] + re[width*(newyres - j)])/2.0;
            }
        }
    }
#endif

    gwy_data_field_resample(rb, width, height, interpolation);
    gwy_data_field_resample(ib, width, height, interpolation);

    g_object_unref(rbuf);
    g_object_unref(ibuf);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

static void
gwy_level_simple(gint n,
                 gint stride,
                 gdouble *data)
{
    gdouble sumxi, sumxixi, sumsi, sumsixi, a, b;
    gdouble *pdata;
    gint i;

    /* These are already averages, not sums */
    sumxi = (n - 1.0)/2.0;
    sumxixi = (2.0*n - 1.0)*(n - 1.0)/6.0;

    sumsi = sumsixi = 0.0;

    pdata = data;
    for (i = n; i; i--) {
        sumsi += *pdata;
        sumsixi += *pdata * i;
        pdata += stride;
    }
    sumsi /= n;
    sumsixi /= n;

    b = (sumsixi - sumsi*sumxi)/(sumxixi - sumxi*sumxi);
    a = (sumsi*sumxixi - sumxi*sumsixi)/(sumxixi - sumxi*sumxi);

    pdata = data;
    for (i = n; i; i--) {
        *pdata -= a + b*i;
        pdata += stride;
    }
}

static inline gdouble
edist(gint xc1, gint yc1, gint xc2, gint yc2)
{
    return sqrt(((gdouble)xc1-xc2)*((gdouble)xc1-xc2)
                + ((gdouble)yc1-yc2)*((gdouble)yc1-yc2));
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
                    mval = edist(0, 0, i, j);
                else
                    mval = edist(0, yres, i, j);
            }
            else {
                if (j < yresh)
                    mval = edist(xres, 0, i, j);
                else
                    mval = edist(xres, yres, i, j);
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

void
gwy_data_field_fft_filter_1d(GwyDataField *data_field,
                             GwyDataField *result_field,
                             GwyDataLine *weights,
                             GwyOrientation orientation,
                             GwyInterpolationType interpolation)
{
    gint i, j;
    GwyDataField *idata_field, *hlp_dfield, *hlp_idfield, *iresult_field;
    GwyDataLine *dline;

    idata_field = gwy_data_field_new_alike(data_field, TRUE);
    hlp_dfield = gwy_data_field_new_alike(data_field, TRUE);
    hlp_idfield = gwy_data_field_new_alike(data_field, TRUE);
    iresult_field = gwy_data_field_new_alike(data_field, TRUE);

    dline = GWY_DATA_LINE(gwy_data_line_new(data_field->xres, data_field->xres,
                                            FALSE));

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(data_field, G_PI/2, interpolation);

    gwy_data_field_area_xfft(data_field, result_field,
                             hlp_dfield, hlp_idfield,
                             0, 0, data_field->xres, data_field->yres,
                             GWY_WINDOWING_NONE,
                             GWY_TRANSFORM_DIRECTION_FORWARD,
                             interpolation,
                             FALSE, FALSE);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(data_field, -G_PI/2, interpolation);

    gwy_data_line_resample(weights, hlp_dfield->xres/2, interpolation);
    for (i = 0; i < hlp_dfield->yres; i++) {
        gwy_data_field_get_row(hlp_dfield, dline, i);
        for (j = 0; j < weights->res; j++) {
            dline->data[j] *= weights->data[j];
            dline->data[dline->res - j - 1] *= weights->data[j];
        }
        gwy_data_field_set_row(hlp_dfield, dline, i);

        gwy_data_field_get_row(hlp_idfield, dline, i);
        for (j = 0; j < weights->res; j++) {
            dline->data[j] *= weights->data[j];
            dline->data[dline->res - j - 1] *= weights->data[j];
        }
        gwy_data_field_set_row(hlp_idfield, dline, i);
    }

    gwy_data_field_area_xfft(hlp_dfield, hlp_idfield,
                             result_field, iresult_field,
                             0, 0, data_field->xres, data_field->yres,
                             GWY_WINDOWING_NONE,
                             GWY_TRANSFORM_DIRECTION_BACKWARD,
                             interpolation,
                             FALSE, FALSE);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(result_field, -G_PI/2, interpolation);

    g_object_unref(idata_field);
    g_object_unref(hlp_dfield);
    g_object_unref(hlp_idfield);
    g_object_unref(iresult_field);
    g_object_unref(dline);
}

/************************** Documentation ****************************/

/**
 * SECTION:inttrans
 * @title: inttrans
 * @short_description: FFT and other integral transforms
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
