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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "linestats.h"

/**
 * gwy_data_line_acf:
 * @data_line: data line
 * @target_line: result data line
 *
 * Coputes autocorrelation function and stores the values in
 * @target_line
 **/
void
gwy_data_line_acf(GwyDataLine *data_line, GwyDataLine *target_line)
{
    gint i, j;
    gint n = data_line->res;
    gdouble val, avg;

    gwy_data_line_resample(target_line, n, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(target_line, 0);
    avg = gwy_data_line_get_avg(data_line);


    for (i = 0; i < n; i++) {
        for (j = 0; j < (n-i); j++) {
            val = (data_line->data[i+j]-avg)*(data_line->data[i]-avg);
            target_line->data[j] += val; /*printf("val=%f\n", val);*/

        }
    }
    for (i = 0; i < n; i++)
        target_line->data[i]/=(n-i);
}

/**
 * gwy_data_line_hhcf:
 * @data_line: data line
 * @target_line: result data line
 *
 * Computes height-height correlation function and stores results in
 * @target_line.
 **/
void
gwy_data_line_hhcf(GwyDataLine *data_line, GwyDataLine *target_line)
{
    gint i, j;
    gint n = data_line->res;
    gdouble val;

    gwy_data_line_resample(target_line, n, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(target_line, 0);

    for (i = 0; i < n; i++) {
        for (j = 0; j < (n-i); j++) {
            val = data_line->data[i+j] - data_line->data[i];
            target_line->data[j] += val*val;
        }
    }
    for (i = 0; i < n; i++)
        target_line->data[i] /= (n-i);
}

/**
 * gwy_data_line_psdf:
 * @data_line: data line
 * @target_line: result data line
 * @windowing: windowing method
 * @interpolation: interpolation method
 *
 * Copmutes power spectral density function and stores the values in
 * @target_line.
 **/
void
gwy_data_line_psdf(GwyDataLine *data_line, GwyDataLine *target_line, gint windowing, gint interpolation)
{
    GwyDataLine *iin, *rout, *iout;
    gint i, order, newres, oldres;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));

    oldres = data_line->res;
    order = (gint) floor(log ((gdouble)data_line->res)/log (2.0)+0.5);
    newres = (gint) pow(2,order);

    iin = (GwyDataLine *)gwy_data_line_new(newres, data_line->real, TRUE);
    rout = (GwyDataLine *)gwy_data_line_new(newres, data_line->real, TRUE);
    iout = (GwyDataLine *)gwy_data_line_new(newres, data_line->real, TRUE);

    /*resample to 2^N (this could be done within FFT, but with loss of precision)*/
    gwy_data_line_resample(data_line, newres, interpolation);

    gwy_data_line_fft(data_line, iin, rout, iout, gwy_data_line_fft_hum,
                   windowing, 1, interpolation,
                   TRUE, TRUE);

    gwy_data_line_resample(target_line, newres/2.0, GWY_INTERPOLATION_NONE);

    /*compute module*/
    for (i = 0; i < (newres/2); i++) {
        target_line->data[i] = (rout->data[i]*rout->data[i] + iout->data[i]*iout->data[i])
            *data_line->real/(newres*newres*2*G_PI);
    }
    target_line->real = 2*G_PI*target_line->res/data_line->real;

    /*resample to given output size*/
    gwy_data_line_resample(target_line, oldres/2.0, interpolation);


    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);

}

/**
 * gwy_data_line_dh:
 * @data_line: data line
 * @target_line: result data line
 * @ymin: minimum value
 * @ymax: maimum value
 * @nsteps: number of histogram steps
 *
 * Computes distribution of heights in interval (@ymin - @ymax).
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum value.
 **/
void
gwy_data_line_dh(GwyDataLine *data_line, GwyDataLine *target_line, gdouble ymin, gdouble ymax, gint nsteps)
{
    gint i, n, val;
    gdouble step, nstep, imin;
    n = data_line->res;

    gwy_data_line_resample(target_line, nsteps, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(target_line, 0);

    /*if ymin==ymax==0 we want to set up histogram area*/
    if ((ymin == ymax) && (ymin == 0))
    {
        ymin = gwy_data_line_get_min(data_line);
        ymax = gwy_data_line_get_max(data_line);
    }
    step = (ymax - ymin)/(nsteps-1);
    imin = (ymin/step);

    for (i=0; i<n; i++)
    {
        val = (gint)((data_line->data[i]/step) - imin);
        if (val<0 || val>= nsteps)
        {
            /*this should never happened*/
            val = 0;
        }
        target_line->data[val] += 1.0;
    }
    nstep = n*step;

    for (i=0; i<nsteps; i++) {target_line->data[i]/=nstep;}
    target_line->real = ymax - ymin;
}

/**
 * gwy_data_line_cdh:
 * @data_line:  data line
 * @target_line: result data line
 * @ymin: minimum value
 * @ymax: maximum value
 * @nsteps: number of histogram steps
 *
 * Computes cumulative distribution of heighs in interval (@ymin - @ymax).
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum value.
 *
 **/
void
gwy_data_line_cdh(GwyDataLine *data_line, GwyDataLine *target_line, gdouble ymin, gdouble ymax, gint nsteps)
{
    gint i;
    gdouble sum = 0;

    gwy_data_line_dh(data_line, target_line, ymin, ymax, nsteps);

    for (i = 0; i < nsteps; i++) {
        sum += target_line->data[i];
        target_line->data[i] = sum;
    }
}

/**
 * gwy_data_line_da:
 * @data_line: data line
 * @target_line: result data line
 * @ymin: minimum value
 * @ymax: maimum value
 * @nsteps: number of angular histogram steps
 *
 * Computes distribution of angles in interval (@ymin - @ymax).
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum angle value.
 **/
void
gwy_data_line_da(GwyDataLine *data_line, GwyDataLine *target_line, gdouble ymin, gdouble ymax, gint nsteps)
{
    /*not yet...*/
    gint i, n, val;
    gdouble step, angle, imin;

    n = data_line->res;
    gwy_data_line_resample(target_line, nsteps, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(target_line, 0);


    /*if ymin==ymax==0 we want to set up histogram area*/
    if ((ymin == ymax) && (ymin == 0))
    {
        ymin = G_MAXDOUBLE;
        ymax = -G_MAXDOUBLE;
        for (i = 0; i < n; i++) {
            angle = gwy_data_line_get_der(data_line, i);
            if (ymin > angle)
                ymin = angle;
            if (ymax < angle)
                ymax = angle;
        }
    }
    step = (ymax - ymin)/(nsteps-1);
    imin = (ymin/step);

    for (i = 0; i < n; i++) {
        val = (gint)(gwy_data_line_get_der(data_line, i)/step - imin);
        if (val < 0)
            val = 0; /*this should never happened*/
        if (val >= nsteps)
            val = nsteps-1; /*this should never happened*/
        target_line->data[val] += 1.0;/*/n/step;*/
    }
    target_line->real = ymax - ymin;
}

/**
 * gwy_data_line_cda:
 * @data_line: data line
 * @target_line: result data line
 * @ymin: minimum value
 * @ymax: maimum value
 * @nsteps: number of angular histogram steps
 *
 * Computes cumulative distribution of angles in interval (@ymin - @ymax).
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum angle value.
 **/
void
gwy_data_line_cda(GwyDataLine *data_line, GwyDataLine *target_line, gdouble ymin, gdouble ymax, gint nsteps)
{
    gint i;
    gdouble sum=0;
    gwy_data_line_da(data_line, target_line, ymin, ymax, nsteps);

    for (i = 0; i < nsteps; i++) {
        sum += target_line->data[i];
        target_line->data[i] = sum;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
