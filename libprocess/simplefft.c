
#include <stdio.h>
#include <math.h>
#include "simplefft.h"

gint 
gwy_fft_hum(gint dir, gdouble *re_in, gdouble *im_in, gdouble *re_out, gdouble *im_out, gint n)
{
    gdouble rc, ic, rt, it, fact;
    gint m, l, i, j, is;
    gdouble imlt;

    imlt = dir * G_PI;
    fact = 1 / sqrt(n);
    j=1;
    for ( i = 1; i <= n; i++ )
    {
	if ( i <= j )
	{
	    rt = fact * re_in[i-1]; it = fact * im_in[i-1]; 
	    re_out[i-1] = fact * re_in[j-1]; im_out[i-1] = fact * im_in[j-1];
	    re_out[j-1] = rt; im_out[j-1] = it;
	}
	m = n >> 1;
	while ((j>m) && m)
	{
	    j -=m; m >>= 1;
	}
	j += m;
	
    }
 
    l=1;
    while (l<n){
	is = l << 1; 
	for (m=1; m<=l; m++)
	{
	    rc = cos(imlt * (m - 1)/l); ic = sin(imlt * (m - 1)/l);  
	    for (i=m; i<=n; i+=is)
	    { 
		rt = rc*re_out[i+l-1] - ic*im_out[i+l-1];
		it = rc*im_out[i+l-1] + ic*re_out[i+l-1]; 
		re_out[i+l-1] = re_out[i-1] - rt;
		im_out[i+l-1] = im_out[i-1] - it;
		re_out[i-1] += rt;
		im_out[i-1] += it; 
	    }
	}
	l = is;
    }
    return 0;
}

gdouble 
gwy_fft_window_hann(gint i, gint n)
{
    return 0.5-0.5*(cos(2*G_PI*i/(n-1)));
}

gdouble 
gwy_fft_window_hamming(gint i, gint n)
{
    return 0.54-0.46*(cos(2*G_PI*i/n));
}

gdouble 
gwy_fft_window_blackmann(gint i, gint n)
{
    gdouble n_2=((gdouble)n)/2;
    return 0.42+0.5*cos(G_PI*(i-n_2)/n_2)+0.08*cos(2*G_PI*(i-n_2)/n_2);
}

gdouble 
gwy_fft_window_lanczos(gint i, gint n)
{
    gdouble n_2=((gdouble)n)/2;
    return sin(G_PI*(i-n_2)/n_2)/(G_PI*(i-n_2)/n_2);
}

gdouble 
gwy_fft_window_welch(gint i, gint n)
{
    gdouble n_2=((gdouble)n)/2;
    return 1-((i-n_2)*(i-n_2)/n_2/n_2);
}

gdouble 
gwy_fft_window_rect(gint i, gint n)
{
    gdouble par;
    if (i==0 || i==(n-1)) par=0.5;
    else par=1;
    return par;
}

void 
gwy_fft_mult(gdouble *data, gint n, gdouble (*p_window)())
{
    gint i;
    for (i=0; i<n; i++) data[i] *= (*p_window)(i, n);
}

gint 
gwy_fft_window(gdouble *data, gint n, gint wintype)
{
    if (wintype == GWY_WINDOW_HANN) gwy_fft_mult(data, n, gwy_fft_window_hann);
    return 0;
}


