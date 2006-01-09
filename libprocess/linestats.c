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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/linestats.h>

/**
 * gwy_data_line_get_max:
 * @data_line: A data line.
 *
 * Finds the maximum value of a data line.
 *
 * Returns: The maximum value.
 **/
gdouble
gwy_data_line_get_max(GwyDataLine *data_line)
{
    gint i;
    gdouble max;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), -G_MAXDOUBLE);

    max = data_line->data[0];
    for (i = 1; i < data_line->res; i++) {
        if (G_UNLIKELY(data_line->data[i] > max))
            max = data_line->data[i];
    }
    return max;
}

/**
 * gwy_data_line_get_min:
 * @data_line: A data line.
 *
 * Finds the minimum value of a data line.
 *
 * Returns: The minimum value.
 **/
gdouble
gwy_data_line_get_min(GwyDataLine *data_line)
{
    gint i;
    gdouble min;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), G_MAXDOUBLE);

    min = data_line->data[0];
    for (i = 1; i < data_line->res; i++) {
        if (G_UNLIKELY(data_line->data[i] < min))
            min = data_line->data[i];
    }
    return min;
}

/**
 * gwy_data_line_get_avg:
 * @data_line: A data line.
 *
 * Computes average value of a data line.
 *
 * Returns: Average value
 **/
gdouble
gwy_data_line_get_avg(GwyDataLine *data_line)
{
    gint i;
    gdouble avg = 0;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);

    for (i = 0; i < data_line->res; i++)
        avg += data_line->data[i];

    return avg/(gdouble)data_line->res;
}

/**
 * gwy_data_line_get_rms:
 * @data_line: A data line.
 *
 * Computes root mean square value of a data line.
 *
 * Returns: Root mean square deviation of values.
 **/
gdouble
gwy_data_line_get_rms(GwyDataLine *data_line)
{
    gint i;
    gdouble sum2 = 0;
    gdouble sum;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);

    sum = gwy_data_line_get_sum(data_line);
    for (i = 0; i < data_line->res; i++)
        sum2 += data_line->data[i]*data_line->data[i];

    return sqrt(fabs(sum2 - sum*sum/data_line->res)/data_line->res);
}

/**
 * gwy_data_line_get_sum:
 * @data_line: A data line.
 *
 * Computes sum of all values in a data line.
 *
 * Returns: sum of all the values.
 **/
gdouble
gwy_data_line_get_sum(GwyDataLine *data_line)
{
    gint i;
    gdouble sum = 0;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);

    for (i = 0; i < data_line->res; i++)
        sum += data_line->data[i];

    return sum;
}

/**
 * gwy_data_line_part_get_max:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Finds the maximum value of a part of a data line.
 *
 * Returns: Maximum within given interval.
 **/
gdouble
gwy_data_line_part_get_max(GwyDataLine *a,
                           gint from, gint to)
{
    gint i;
    gdouble max = -G_MAXDOUBLE;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), max);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, max);

    for (i = from; i < to; i++) {
        if (max < a->data[i])
            max = a->data[i];
    }
    return max;
}

/**
 * gwy_data_line_part_get_min:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Finds the minimum value of a part of a data line.
 *
 * Returns: Minimum within given interval.
 **/
gdouble
gwy_data_line_part_get_min(GwyDataLine *a,
                           gint from, gint to)
{
    gint i;
    gdouble min = G_MAXDOUBLE;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), min);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, min);

    for (i = from; i < to; i++) {
        if (min > a->data[i])
            min = a->data[i];
    }

    return min;
}

/**
 * gwy_data_line_part_get_avg:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Computes mean value of all values in a part of a data line.
 *
 * Returns: Average value within given interval.
 **/
gdouble
gwy_data_line_part_get_avg(GwyDataLine *a, gint from, gint to)
{
    return gwy_data_line_part_get_sum(a, from, to)/(gdouble)(to-from);
}

/**
 * gwy_data_line_part_get_rms:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Computes root mean square value of a part of a data line.
 *
 * Returns: Root mean square deviation of heights within a given interval
 **/
gdouble
gwy_data_line_part_get_rms(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble rms = 0;
    gdouble avg;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), rms);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, rms);

    avg = gwy_data_line_part_get_avg(a, from, to);
    for (i = from; i < to; i++)
        rms += (avg - a->data[i])*(avg - a->data[i]);

    return sqrt(rms)/(gdouble)(to-from);
}

/**
 * gwy_data_line_part_get_sum:
 * @data_line: A data line.
 * @from: Index the line part starts at.
 * @to: Index the line part ends at + 1.
 *
 * Computes sum of all values in a part of a data line.
 *
 * Returns: Sum of all values within the interval.
 **/
gdouble
gwy_data_line_part_get_sum(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble sum = 0;

    g_return_val_if_fail(GWY_IS_DATA_LINE(a), sum);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, sum);

    for (i = from; i < to; i++)
        sum += a->data[i];

    return sum;
}

/**
 * gwy_data_line_acf:
 * @data_line: A data line.
 * @target_line: Data line to store autocorrelation function to.  It will be
 *               resized to @data_line size.
 *
 * Coputes autocorrelation function and stores the values in
 * @target_line
 **/
void
gwy_data_line_acf(GwyDataLine *data_line, GwyDataLine *target_line)
{
    gint i, j, n;
    gdouble val, avg;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    n = data_line->res;
    gwy_data_line_resample(target_line, n, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    avg = gwy_data_line_get_avg(data_line);
    target_line->real = data_line->real;
    target_line->off = 0.0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < (n-i); j++) {
            val = (data_line->data[i+j] - avg)*(data_line->data[i] - avg);
            target_line->data[j] += val;

        }
    }
    for (i = 0; i < n; i++)
        target_line->data[i] /= n-i;
}

/**
 * gwy_data_line_hhcf:
 * @data_line: A data line.
 * @target_line: Data line to store height-height function to.  It will be
 *               resized to @data_line size.
 *
 * Computes height-height correlation function and stores results in
 * @target_line.
 **/
void
gwy_data_line_hhcf(GwyDataLine *data_line, GwyDataLine *target_line)
{
    gint i, j, n;
    gdouble val;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    n = data_line->res;
    gwy_data_line_resample(target_line, n, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    target_line->real = data_line->real;
    target_line->off = 0.0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < (n-i); j++) {
            val = data_line->data[i+j] - data_line->data[i];
            target_line->data[j] += val*val;
        }
    }
    for (i = 0; i < n; i++)
        target_line->data[i] /= n-i;
}

/**
 * gwy_data_line_psdf:
 * @data_line: A data line.  Its contents is destroyes and even its size
 *             may change.
 * @target_line: Data line to store power spectral density function to.
 *               It will be resized to @data_line size.
 * @windowing: Windowing method to use.
 * @interpolation: Interpolation method to use.
 *
 * Copmutes power spectral density function and stores the values in
 * @target_line.
 **/
void
gwy_data_line_psdf(GwyDataLine *data_line,
                   GwyDataLine *target_line,
                   gint windowing,
                   gint interpolation)
{
    GwyDataLine *iin, *rout, *iout;
    gint i, order, newres, oldres;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    oldres = data_line->res;
    order = ROUND(log(data_line->res)/G_LN2);
    newres = (gint)pow(2, order);

    iin = gwy_data_line_new(newres, data_line->real, TRUE);
    rout = gwy_data_line_new(newres, data_line->real, TRUE);
    iout = gwy_data_line_new(newres, data_line->real, TRUE);

    /* resample to 2^N (this could be done within FFT, but with loss of
     * precision)*/
    gwy_data_line_resample(data_line, newres, interpolation);

    gwy_data_line_fft(data_line, iin, rout, iout,
                      windowing,
                      GWY_TRANSFORM_DIRECTION_FORWARD,
                      interpolation,
                      TRUE, TRUE);

    gwy_data_line_resample(target_line, newres/2.0, GWY_INTERPOLATION_NONE);

    /*compute module*/
    for (i = 0; i < newres/2; i++) {
        target_line->data[i] = (rout->data[i]*rout->data[i]
                                + iout->data[i]*iout->data[i])
                               *data_line->real/(newres*newres*2*G_PI);
    }
    target_line->real = 2*G_PI*target_line->res/data_line->real;
    target_line->off = 0.0;

    /*resample to given output size*/
    gwy_data_line_resample(target_line, oldres/2.0, interpolation);

    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);
}

/**
 * gwy_data_line_dh:
 * @data_line: A data line.
 * @target_line: Data line to store height distribution function to.
 *               It will be resized to @nsteps.
 * @ymin: Height distribution minimum value.
 * @ymax: Height distribution maximum value.
 * @nsteps: Number of histogram steps.
 *
 * Computes distribution of heights in interval [@ymin, @ymax).
 *
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum value.
 **/
void
gwy_data_line_dh(GwyDataLine *data_line,
                 GwyDataLine *target_line,
                 gdouble ymin, gdouble ymax,
                 gint nsteps)
{
    gint i, n, val;
    gdouble step, nstep, imin;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    n = data_line->res;
    gwy_data_line_resample(target_line, nsteps, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);

    /* if ymin == ymax == 0 we want to set up histogram area */
    if (!ymin && !ymax) {
        ymin = gwy_data_line_get_min(data_line);
        ymax = gwy_data_line_get_max(data_line);
    }
    step = (ymax - ymin)/(nsteps - 1.0);
    imin = ymin/step;

    for (i = 0; i < n; i++) {
        val = (gint)((data_line->data[i]/step) - imin);
        if (G_UNLIKELY(val < 0 || val >= nsteps)) {
            /*this should never happened*/
            val = 0;
        }
        target_line->data[val] += 1.0;
    }
    nstep = n*step;
    gwy_data_line_multiply(target_line, 1.0/nstep);
    target_line->off = ymin;
    target_line->real = ymax - ymin;
}

/**
 * gwy_data_line_cdh:
 * @data_line: A data line.
 * @target_line: Data line to store height distribution function to.
 *               It will be resized to @nsteps.
 * @ymin: Height distribution minimum value.
 * @ymax: Height distribution maximum value.
 * @nsteps: Number of histogram steps.
 *
 * Computes cumulative distribution of heighs in interval [@ymin, @ymax).
 *
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum value.
 **/
void
gwy_data_line_cdh(GwyDataLine *data_line,
                  GwyDataLine *target_line,
                  gdouble ymin, gdouble ymax,
                  gint nsteps)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    gwy_data_line_dh(data_line, target_line, ymin, ymax, nsteps);
    gwy_data_line_cumulate(target_line);
}

/**
 * gwy_data_line_da:
 * @data_line: A data line.
 * @target_line: Data line to store angle distribution function to.
 * @ymin: Angle distribution minimum value.
 * @ymax: Angle distribution maximum value.
 * @nsteps: Mumber of angular histogram steps.
 *
 * Computes distribution of angles in interval [@ymin, @ymax).
 *
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum angle value.
 **/
void
gwy_data_line_da(GwyDataLine *data_line,
                 GwyDataLine *target_line,
                 gdouble ymin, gdouble ymax,
                 gint nsteps)
{
    gint i, n, val;
    gdouble step, angle, imin;

    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    n = data_line->res;
    gwy_data_line_resample(target_line, nsteps, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);

    /* if ymin == ymax == 0 we want to set up histogram area */
    if (!ymin && !ymax) {
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
    step = (ymax - ymin)/(nsteps - 1.0);
    imin = ymin/step;

    for (i = 0; i < n; i++) {
        val = (gint)(gwy_data_line_get_der(data_line, i)/step - imin);
        if (G_UNLIKELY(val < 0))
            val = 0; /* this should never happened */
        if (G_UNLIKELY(val >= nsteps))
            val = nsteps-1; /* this should never happened */
        target_line->data[val] += 1.0;
    }
    target_line->real = ymax - ymin;
    target_line->off = ymin;
}

/**
 * gwy_data_line_cda:
 * @data_line: A data line.
 * @target_line: Data line to store angle distribution function to.
 *               It will be resized to @nsteps.
 * @ymin: Angle distribution minimum value.
 * @ymax: Angle distribution maximum value.
 * @nsteps: Number of angular histogram steps.
 *
 * Computes cumulative distribution of angles in interval [@ymin, @ymax).
 *
 * If the interval is (0, 0) it computes the distribution from
 * real data minimum and maximum angle value.
 **/
void
gwy_data_line_cda(GwyDataLine *data_line,
                  GwyDataLine *target_line,
                  gdouble ymin, gdouble ymax,
                  gint nsteps)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));

    gwy_data_line_da(data_line, target_line, ymin, ymax, nsteps);
    gwy_data_line_cumulate(target_line);
}

/**
 * gwy_data_line_get_length:
 * @data_line: A data line to compute length of.
 *
 * Calculates physical length of a data line.
 *
 * The length is calculated from approximation by straight segments between
 * values.
 *
 * Returns: The line length.
 **/
gdouble
gwy_data_line_get_length(GwyDataLine *data_line)
{
    gdouble sum, q;
    gint i, n;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0.0);

    n = data_line->res;
    q = data_line->real/n;
    if (G_UNLIKELY(data_line->res == 1))
        return q;

    sum = 0.0;
    for (i = 1; i < n; i++)
        sum += hypot(q, data_line->data[i] - data_line->data[i-1]);

    /* We calculate length of inner part of a segment.  If we assume the
     * average properties of border are the same as of the inner part,
     * we can simply multiply the sum with the total/inner length ratio */
    sum *= n/(n - 1.0);

    return sum;
}

/**
 * gwy_data_line_part_get_modus:
 * @data_line: A data line.
 * @from: The index in @data_line to start from (inclusive).
 * @to: The index in @data_line to stop (noninclusive).
 * @histogram_steps: Number of histogram steps used for modus searching,
 *                   pass a nonpositive number to autosize.
 *
 * Finds approximate modus of a data line part.
 *
 * Returns: The modus.
 **/
gdouble
gwy_data_line_part_get_modus(GwyDataLine *data_line,
                             gint from, gint to,
                             gint histogram_steps)
{
    gint *histogram;
    gint n, i, j, m;
    gdouble min, max, sum;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);
    g_return_val_if_fail(from >= 0 && to <= data_line->res, 0);
    g_return_val_if_fail(from != to, 0);

    if (from > to)
        GWY_SWAP(gint, from, to);
    n = to - from;

    if (n == 1)
        return data_line->data[from];

    if (histogram_steps < 1) {
        /*
        gdouble sigma = gwy_data_line_part_get_rms(data_line, from, to);
        histogram_steps = floor(0.49*sigma*cbrt(n) + 0.5);
        */
        histogram_steps = floor(3.49*cbrt(n) + 0.5);
        gwy_debug("histogram_steps = %d", histogram_steps);
    }

    min = gwy_data_line_part_get_min(data_line, from, to);
    max = gwy_data_line_part_get_max(data_line, from, to);
    if (min == max)
        return min;

    histogram = g_new0(gint, histogram_steps);
    for (i = from; i < to; i++) {
        j = (data_line->data[i] - min)/(max - min)*histogram_steps;
        j = CLAMP(j, 0, histogram_steps-1);
        histogram[j]++;
    }

    m = 0;
    for (i = 1; i < histogram_steps; i++) {
        if (histogram[i] > histogram[m])
            m = i;
    }

    n = 0;
    sum = 0.0;
    for (i = from; i < to; i++) {
        j = (data_line->data[i] - min)/(max - min)*histogram_steps;
        j = CLAMP(j, 0, histogram_steps-1);
        if (j == m) {
            sum += data_line->data[i];
            n++;
        }
    }

    g_free(histogram);
    gwy_debug("modus = %g", sum/n);

    return sum/n;
}

/**
 * gwy_data_line_get_modus:
 * @data_line: A data line.
 * @histogram_steps: Number of histogram steps used for modus searching,
 *                   pass a nonpositive number to autosize.
 *
 * Finds approximate modus of a data line.
 *
 * As each number in the data line is usually unique, this function does not
 * return modus of the data itself, but modus of a histogram.
 *
 * Returns: The modus.
 **/
gdouble
gwy_data_line_get_modus(GwyDataLine *data_line,
                        gint histogram_steps)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);

    return gwy_data_line_part_get_modus(data_line, 0, data_line->res,
                                        histogram_steps);
}

/************************** Documentation ****************************/

/**
 * SECTION:linestats
 * @title: linestats
 * @short_description: One-dimensional statistical functions
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
