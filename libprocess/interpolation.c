/* @(#) $Id$ */

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


