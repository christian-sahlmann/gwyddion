/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <stdio.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "interpolation.h"

/*simple interpolation of non-equidistant values using two neighbour values*/
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

    if (interpolation == GWY_INTERPOLATION_ROUND) {
        if ((x - x1_) < (x2_ - x))
            return y1_;
        else
            return y2_;
    }
    else if (interpolation==GWY_INTERPOLATION_BILINEAR) {
        return y1_ + (x - x1_)/(x2_ - x1_)*(y2_ - y1_);
    }
    else {
        g_warning("Interpolation not implemented yet.\n");
    }
    return 0;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
