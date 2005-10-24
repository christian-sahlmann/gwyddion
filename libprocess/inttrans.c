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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/cwt.h>

/* INTERPOLATION: New (not applicable). */

static void     gwy_data_field_mult_wav          (GwyDataField *real_field,
                                                  GwyDataField *imag_field,
                                                  gdouble scale,
                                                  Gwy2DCWTWaveletType wtype);
static gdouble  edist                            (gint xc1, gint yc1,
                                                  gint xc2, gint yc2);

/**
 * gwy_data_field_2dfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Computes 2D FFT using a specified 1D alogrithm.
 *
 * This can be for example "gwy_data_line_fft_hum", which is the
 * simplest algoritm avalilable. If requested a windowing
 * and/or leveling is applied to preprocess data to obtain
 * reasonable results.
 **/
void
gwy_data_field_2dfft(GwyDataField *ra, GwyDataField *ia,
                     GwyDataField *rb, GwyDataField *ib,
                     GwyWindowingType windowing,
                     GwyTransformDirection direction,
                     GwyInterpolationType interpolation,
                     gboolean preserverms, gboolean level)
{
    GwyDataField *rh, *ih;

    rh = gwy_data_field_new_alike(ra, FALSE);
    ih = gwy_data_field_new_alike(ra, FALSE);
    gwy_data_field_xfft(ra, ia, rh, ih,
                        windowing, direction, interpolation,
                        preserverms, level);
    gwy_data_field_yfft(rh, ih, rb, ib,
                        windowing, direction, interpolation,
                        preserverms, level);

    g_object_unref(rh);
    g_object_unref(ih);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_2dfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Computes 2D FFT using a specified 1D algorithm.
 *
 * As the input is only real, the computation can be a little bit
 * faster.
 **/
void
gwy_data_field_2dfft_real(GwyDataField *ra, GwyDataField *rb,
                          GwyDataField *ib,
                          GwyWindowingType windowing,
                          GwyTransformDirection direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms, gboolean level)
{
    GwyDataField *rh, *ih;

    rh = gwy_data_field_new_alike(ra, FALSE);
    ih = gwy_data_field_new_alike(ra, FALSE);
    gwy_data_field_xfft_real(ra, rh, ih,
                             windowing, direction, interpolation,
                             preserverms, level);
    gwy_data_field_yfft(rh, ih, rb, ib,
                        windowing, direction, interpolation,
                        preserverms, level);

    g_object_unref(rh);
    g_object_unref(ih);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}


/**
 * gwy_data_field_get_fft_res:
 * @data_res: data resolution
 *
 * Finds the closest 2^N value.
 *
 * Returns: 2^N good for FFT.
 **/
gint
gwy_data_field_get_fft_res(gint data_res)
{
    return (gint)pow(2, ((gint)floor(log((gdouble)data_res)/log(2.0) + 0.5)));
}


/**
 * gwy_data_field_2dfft_humanize:
 * @a: A data field
 *
 * Swap top-left, top-right, bottom-left and bottom-right
 * squares to obtain a humanized 2D FFT output with 0,0 in the center.
 **/
void
gwy_data_field_2dfft_humanize(GwyDataField *a)
{
    gint i, j, im, jm, xres;
    GwyDataField *b;

    b = gwy_data_field_duplicate(a);
    im = a->yres/2;
    jm = a->xres/2;
    xres = a->xres;
    for (i = 0; i < im; i++) {
        for (j = 0; j < jm; j++) {
            a->data[(j + jm) + (i + im)*xres] = b->data[j + i*xres];
            a->data[(j + jm) + i*xres] = b->data[j + (i + im)*xres];
            a->data[j + (i + im)*xres] = b->data[(j + jm) + i*xres];
            a->data[j + i*xres] = b->data[(j + jm) + (i + im)*xres];
        }
    }
    g_object_unref(b);
}

/**
 * gwy_data_field_xfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all rows in the data field using 1D algorithm
 * and other parameters specified.
 *
 **/
void
gwy_data_field_xfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    GwyWindowingType windowing,
                    GwyTransformDirection direction,
                    GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine *rin, *iin, *rout, *iout;

    rin = gwy_data_line_new(ra->xres, ra->xreal, FALSE);
    rout = gwy_data_line_new(ra->xres, ra->xreal, FALSE);
    iin = gwy_data_line_new(ra->xres, ra->xreal, FALSE);
    iout = gwy_data_line_new(ra->xres, ra->xreal, FALSE);

    gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    for (k = 0; k < ra->yres; k++) {
        gwy_data_field_get_row(ra, rin, k);
        gwy_data_field_get_row(ia, iin, k);

        gwy_data_line_fft(rin, iin, rout, iout,
                          windowing, direction, interpolation,
                          preserverms, level);

        gwy_data_field_set_row(rb, rout, k);
        gwy_data_field_set_row(ib, iout, k);
    }

    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_yfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all columns in the data field using 1D algorithm
 * and other parameters specified.
 *
 **/
void
gwy_data_field_yfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    GwyWindowingType windowing,
                    GwyTransformDirection direction,
                    GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine *rin, *iin, *rout, *iout;

    rin = gwy_data_line_new(ra->yres, ra->yreal, FALSE);
    rout = gwy_data_line_new(ra->yres, ra->yreal, FALSE);
    iin = gwy_data_line_new(ra->yres, ra->yreal, FALSE);
    iout = gwy_data_line_new(ra->yres, ra->yreal, FALSE);

    gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    /*we compute each two FFTs simultaneously*/
    for (k = 0; k < ra->xres; k++) {
        gwy_data_field_get_column(ra, rin, k);
        gwy_data_field_get_column(ia, iin, k);
        gwy_data_line_fft(rin, iin, rout, iout,
                          windowing, direction, interpolation,
                          preserverms, level);
        gwy_data_field_set_column(rb, rout, k);
        gwy_data_field_set_column(ib, iout, k);
    }

    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_xfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all rows in the data field using 1D algorithm
 * and other parameters specified. Only real input field
 * is used, so computation can be faster.
 *
 **/
void
gwy_data_field_xfft_real(GwyDataField *ra, GwyDataField *rb,
                         GwyDataField *ib,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gboolean level)
{
    gint k, j;
    GwyDataLine *rin, *iin, *rout, *iout, *rft1, *ift1, *rft2, *ift2;

    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    rin = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rout = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    iin = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    iout = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rft1 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    ift1 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rft2 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    ift2 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);

    /*we compute allways two FFTs simultaneously*/
    for (k = 0; k < ra->yres; k++) {
        gwy_data_field_get_row(ra, rin, k);
        if (k < ra->yres-1)
            gwy_data_field_get_row(ra, iin, k+1);
        else {
            gwy_data_field_get_row(ra, iin, k);
            gwy_data_line_clear(iin);
        }

        gwy_data_line_fft(rin, iin, rout, iout,
                          windowing, direction, interpolation,
                          preserverms, level);

        /*extract back the two profiles FFTs*/
        rft1->data[0] = rout->data[0];
        ift1->data[0] = 0;
        rft2->data[0] = iout->data[0];
        ift2->data[0] = 0;
        for (j = 1; j < ra->xres; j++) {
            rft1->data[j] = (rout->data[j] + rout->data[ra->xres - j])/2;
            ift1->data[j] = (iout->data[j] - iout->data[ra->xres - j])/2;
            rft2->data[j] = (iout->data[j] + iout->data[ra->xres - j])/2;
            ift2->data[j] = -(rout->data[j] - rout->data[ra->xres - j])/2;
        }

        gwy_data_field_set_row(rb, rft1, k);
        gwy_data_field_set_row(ib, ift1, k);

        if (k < ra->yres-1) {
            gwy_data_field_set_row(rb, rft2, k+1);
            gwy_data_field_set_row(ib, ift2, k+1);
            k++;
        }
    }

    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);
    g_object_unref(rft1);
    g_object_unref(rft2);
    g_object_unref(ift1);
    g_object_unref(ift2);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
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

    gwy_data_field_xfft(data_field, result_field,
                        hlp_dfield, hlp_idfield,
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

    gwy_data_field_xfft(hlp_dfield, hlp_idfield,
                        result_field, iresult_field,
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
