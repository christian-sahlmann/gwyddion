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
#include <libprocess/interpolation.h>

/* INTERPOLATION: New (not applicable). */

/**
 * gwy_interpolation_get_dval:
 * @x: requested value coordinate
 * @x1_: x coordinate of first value
 * @y1_: y coordinate of first value
 * @x2_: x coordinate of second value
 * @y2_: y coordinate of second value
 * @interpolation: interpolation type
 *
 * This function uses two-point interpolation
 * methods to get interpolated value between
 * two arbitrary data points.
 *
 * Returns: interpolated value
 **/
gdouble
gwy_interpolation_get_dval(gdouble x,
                           gdouble x1_, gdouble y1_,
                           gdouble x2_, gdouble y2_,
                           GwyInterpolationType interpolation)
{
    if (x1_ > x2_) {
        GWY_SWAP(gdouble, x1_, x2_);
        GWY_SWAP(gdouble, y1_, y2_);
    }

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        if ((x - x1_) < (x2_ - x))
            return y1_;
        else
            return y2_;
        break;


        case GWY_INTERPOLATION_BILINEAR:
        return y1_ + (x - x1_)/(x2_ - x1_)*(y2_ - y1_);
        break;

        default:
        g_warning("Interpolation not implemented yet.\n");
        break;
    }
    return 0.0;
}

/**
 * gwy_interpolation_get_dval_of_equidists:
 * @x: Noninteger part of requested x, that is a number from interval [0,1).
 * @data: Array of 4 values to interpolate between (see below).
 * @interpolation: Interpolation type to use.
 *
 * Computes interpolated value from 2 or 4 equidistant values.
 *
 * For %GWY_INTERPOLATION_NONE no @data value is actually used, and zero is
 * returned.
 *
 * For %GWY_INTERPOLATION_ROUND or %GWY_INTERPOLATION_BILINEAR
 * it is enough to set middle two @data values, that to use @data in format
 * {0, data[i], data[i+1], 0} and function computes value at data[i+x]
 * (the outer values are not used).
 *
 * For four value interpolations you have to prepare @data as
 * {data[i-1], data[i], data[i+1], data[i+2]} and function again
 * returns value at data[i+x].
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolation_get_dval_of_equidists(gdouble x,
                                        gdouble *data,
                                        GwyInterpolationType interpolation)
{


    gint l;
    gdouble w1, w2, w3, w4;
    gdouble rest;

    x += 1.0;
    l = floor(x);
    rest = x - (gdouble)l;

    g_return_val_if_fail(x >= 1 && x < 2, 0.0);

    if (rest == 0)
        return data[l];

    /*simple (and fast) methods*/
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        return data[(gint)(x + 0.5)];

        case GWY_INTERPOLATION_BILINEAR:
        return
            (1.0 - rest)*data[l] + rest*data[l+1];

        default:
        break;
    }

    switch (interpolation) {
        case GWY_INTERPOLATION_KEY:
        /*
        w1 = rest + 1.0;
        w2 = rest;
        w3 = 1.0 - rest;
        w4 = 2.0 - rest;
        w1 = -0.5*w1*w1*w1 + 2.5*w1*w1 - 4*w1 + 2;
        w2 = 1.5*w2*w2*w2 - 2.5*w2*w2 + 1;
        w3 = 1.5*w3*w3*w3 - 2.5*w3*w3 + 1;
        w4 = -0.5*w4*w4*w4 + 2.5*w4*w4 - 4*w4 + 2;
        */
        /* horner schema (by maple) */
        w1 = (-0.5 + (1.0 - rest/2.0)*rest)*rest;
        w2 = 1.0 + (-2.5 + 1.5*rest)*rest*rest;
        w3 = (0.5 + (2.0 - 1.5*rest)*rest)*rest;
        w4 = (-0.5 + rest/2.0)*rest*rest;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        /*
        w1 = rest + 1.0;
        w2 = rest;
        w3 = 1.0 - rest;
        w4 = 2.0 - rest;
        w1 = (2-w1)*(2-w1)*(2-w1)/6;
        w2 = 0.6666667-0.5*w2*w2*(2-w2);
        w3 = 0.6666667-0.5*w3*w3*(2-w3);
        w4 = (2-w4)*(2-w4)*(2-w4)/6;
        */
        /* horner schema (by maple) */
        w1 = 1.0-rest;
        w1 = w1*w1*w1/6.0;
        w2 = 2.0/3.0 + (-1.0 + rest/2.0)*rest*rest;
        w3 = 1.0/6.0 + (0.5 + (0.5 - rest/2.0)*rest)*rest;
        w4 = rest*rest*rest/6.0;
        break;

        case GWY_INTERPOLATION_OMOMS:
        /*
        w1 = rest + 1.0;
        w2 = rest;
        w3 = 1.0 - rest;
        w4 = 2.0 - rest;
        w1 = -w1*w1*w1/6 + w1*w1-85*w1/42 + 1.3809523;
        w2 = w2*w2*w2/2-w2*w2 + w2/14 + 0.6190476;
        w3 = w3*w3*w3/2-w3*w3 + w3/14 + 0.6190476;
        w4 = -w4*w4*w4/6 + w4*w4-85*w4/42 + 1.3809523;
        */
        /* horner schema (by maple) */
        w1 = 4.0/21.0 + (-11.0/21.0 + (0.5 - rest/6.0)*rest)*rest;
        w2 = 13.0/21.0 + (1.0/14.0 + (-1.0 + rest/2.0)*rest)*rest;
        w3 = 4.0/21.0 + (3.0/7.0 + (0.5 - rest/2.0)*rest)*rest;
        w4 = (1.0/42.0 + rest*rest/6.0)*rest;
        break;

        case GWY_INTERPOLATION_NNA:
        w1 = rest + 1.0;
        w2 = rest;
        w3 = 1.0 - rest;
        w4 = 2.0 - rest;
        w1 = 1/(w1*w1*w1*w1);
        w2 = 1/(w2*w2*w2*w2);
        w3 = 1/(w3*w3*w3*w3);
        w4 = 1/(w4*w4*w4*w4);
        return (w1*data[l-1] + w2*data[l]
                + w3*data[l+1] + w4*data[l+2])/(w1 + w2 + w3 + w4);

        default:
        g_assert_not_reached();
        w1 = w2 = w3 = w4 = 0.0;
        break;
    }

    return w1*data[l-1] + w2*data[l] + w3*data[l+1] + w4*data[l+2];
}

/***** Documentation *******************************************************/

/* Maple source of Hornerization.

{VERSION 5 0 "IBM INTEL LINUX" "5.0" }
{USTYLETAB {CSTYLE "Maple Input" -1 0 "Courier" 0 1 255 0 0 1 0 1 0 0
1 0 0 0 0 1 }{PSTYLE "Normal" -1 0 1 {CSTYLE "" -1 -1 "" 0 1 0 0 0 0
0 0 0 0 0 0 0 0 0 0 }0 0 0 -1 -1 -1 0 0 0 0 0 0 -1 0 }}
{SECT 0 {EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 23 "restart; with(codege
n):" }}}{EXCHG {PARA 0 "" 0 "" {TEXT -1 17 "Key interpolation" }}}
{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 146 "intp := [ -1/2*w1*w1*w1 + 5
/2*w1*w1 - 4*w1 + 2, 3/2*w2*w2*w2 - 5/2*w2*w2 + 1, 3/2*w3*w3*w3 - 5/2*
w3*w3 + 1, -1/2*w4*w4*w4 + 5/2*w4*w4 - 4*w4 + 2];" }}{PARA 0 "" 0 ""
{TEXT -1 0 "" }}}{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 88 "hintp := co
nvert(subs(w1 = rest+1, w2 = rest, w3 = 1-rest, w4 = 2-rest, intp), ho
rner); " }}}{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 67 "C([ w1 = hintp[1
], w2 = hintp[2], w3 = hintp[3], w4 = hintp[4] ] );" }}}{EXCHG {PARA
0 "" 0 "" {TEXT -1 21 "Bspline interpolation" }}}{EXCHG {PARA 0 "> "
0 "" {MPLTEXT 1 0 107 "intp := [ (2-w1)*(2-w1)*(2-w1)/6, 2/3 - 1/2*w2*
w2*(2-w2), 2/3 - 1/2*w3*w3*(2-w3), (2-w4)*(2-w4)*(2-w4)/6 ];" }}}
{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 88 "hintp := convert(subs(w1 = r
est+1, w2 = rest, w3 = 1-rest, w4 = 2-rest, intp), horner); " }}}
{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 67 "C([ w1 = hintp[1], w2 = hint
p[2], w3 = hintp[3], w4 = hintp[4] ] );" }}}{EXCHG {PARA 0 "" 0 ""
{TEXT -1 19 "OMOMS interpolation" }}}{EXCHG {PARA 0 "> " 0 ""
{MPLTEXT 1 0 172 "intp := [ -1/6*w1*w1*w1 + w1*w1 - 85/42*w1 + 29/21, \+
1/2*w2*w2*w2 - w2*w2 + w2/14 + 13/21, 1/2*w3*w3*w3 - w3*w3 + w3/14 + 1
3/21, -1/6*w4*w4*w4 + w4*w4 - 85/42*w4 + 29/21 ]; " }}}{EXCHG {PARA 0
"> " 0 "" {MPLTEXT 1 0 88 "hintp := convert(subs(w1 = rest+1, w2 = res
t, w3 = 1-rest, w4 = 2-rest, intp), horner); " }}}{EXCHG {PARA 0 "> "
0 "" {MPLTEXT 1 0 67 "C([ w1 = hintp[1], w2 = hintp[2], w3 = hintp[3],
 w4 = hintp[4] ] );" }}}{EXCHG {PARA 0 "> " 0 "" {MPLTEXT 1 0 0 "" }}}
}{MARK "14" 0 }{VIEWOPTS 1 1 0 1 1 1803 1 1 1 1 }{PAGENUMBERS 0 1 2
33 1 1 }
*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
