/*

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
#include <math.h>
#include "cwt.h"

gdouble 
wfunc_2d(gdouble scale, gdouble mval, gint xres, Gwy2DCWTWaveletType wtype)
{
    gdouble dat2x, cur;
    
    dat2x=4.0/(gdouble)xres;
    cur=mval*dat2x;

    if (wtype==GWY_2DCWT_GAUSS) return exp(-(scale*scale*cur*cur)/2)*2*G_PI*scale*scale*2*G_PI*scale;
    else if (wtype==GWY_2DCWT_HAT) return (scale*scale*cur*cur)*exp(-(scale*scale*cur*cur)/2)*2*G_PI*scale*scale;
    else return 1;
    

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
