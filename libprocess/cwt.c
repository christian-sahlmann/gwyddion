
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
