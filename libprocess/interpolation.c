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

gdouble 
gwy_interpolation_get_dval(gdouble x, gdouble x1, gdouble y1, gdouble x2, gdouble y2, gint interpolation)
{
    if (x1 > x2){GWY_SWAP(gdouble, x1, x2); GWY_SWAP(gdouble, y1, y2);}

    if (interpolation==GWY_INTERPOLATION_ROUND)
    {
	if ((x - x1) < (x2 - x)) return y1;
	else return y2;
    }
    else if (interpolation==GWY_INTERPOLATION_BILINEAR)
    {
	return y1 + (x - x1)/(x2 - x1)*(y2 - y1);
    }
    else {g_warning("Interpolation not implemented yet.\n");}
    return 0;
}


