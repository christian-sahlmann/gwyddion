/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>
#include "gwypixfield.h"


void 
gwy_pixfield_do(GdkPixbuf *g, GwyDataField *f, GwyPalette *pal)
{
    int xres = f->xres;
    int yres = f->yres;
    int i, j;
    guchar *pixels, *line;
    gint rowstride, dval;
    gdouble maximum, minimum, kor;

    maximum = gwy_data_field_get_max(f); 
    minimum = gwy_data_field_get_min(f);
    kor = (pal->nofvals-1)/(maximum-minimum);
   
    pixels = gdk_pixbuf_get_pixels(g);
    rowstride = gdk_pixbuf_get_rowstride(g);
    
    for (i=0; i<(yres); i++)
    {
	line=pixels + i*rowstride;
    	for (j=0; j<(xres); j++)
	{   
	    dval=(gint)((f->data[i + j*f->yres]-minimum)*kor + 0.5);
	    *(line++) = (guchar)((gint)pal->color[dval].r);
	    *(line++) = (guchar)((gint)pal->color[dval].g);
	    *(line++) = (guchar)((gint)pal->color[dval].b);
	}
    }
}



