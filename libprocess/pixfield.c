
#include <stdio.h>
#include <math.h>
#include "pixfield.h"

void gwy_pixfield_presetpal(PixPalette *pal, gint type)
{
    gint i;
    gdouble rv, gv, bv;
    
    if (type == GWY_PAL_BLACK)
    {
	for (i=0; i<512; i++) 
	{
	    pal->r[i]=i/2;
	    pal->g[i]=i/2; if (i%2 == 0 && i<510) pal->g[i]+=1;
	    pal->b[i]=i/2;
	}
    }
    else 
    {
	if (type==GWY_PAL_RED){rv=1; gv=0; bv=0;}
	else if (type==GWY_PAL_GREEN){rv=0; gv=1; bv=0;}
	else if (type==GWY_PAL_BLUE){rv=0; gv=0; bv=1;}
	else if (type==GWY_PAL_YELLOW){rv=0.8313; gv=0.721; bv=0.1625;}
	else if (type==GWY_PAL_PINK){rv=1; gv=0.0666; bv=0.694;}
	else if (type==GWY_PAL_BROWN){rv=0.95; gv=0.62; bv=0.1625;}
	else if (type==GWY_PAL_NAVY){rv=0.31; gv=0.41; bv=1;}
	else if (type==GWY_PAL_OLIVE){rv=0.3725; gv=0.6941; bv=0.4588;}
	else {g_warning("Unknown palette.\n");}
	
	for (i=0; i<256; i++)
	{
	    pal->r[i]=(gint)(i*rv);
	    pal->g[i]=(gint)(i*gv);
	    pal->b[i]=(gint)(i*bv);
	}
	for (i=256; i<512; i++)
	{
	    pal->r[i]=pal->r[255]+(gint)((i-255)*(1-rv));
	    pal->g[i]=pal->g[255]+(gint)((i-255)*(1-gv));
	    pal->b[i]=pal->b[255]+(gint)((i-255)*(1-bv));
	}
    }

}

void gwy_pixfield_do(GdkPixbuf *g, GwyDataField *f, PixPalette *pal)
{
    int xres = f->xres;
    int yres = f->yres;
    int i, j;
    guchar *pixels, *line;
    guchar rval, gval, bval;
    gint rowstride, dval;
    gdouble maximum, minimum, kor;

    maximum=gwy_datafield_get_max(f); 
    minimum=gwy_datafield_get_min(f);
    kor=511.0/(maximum-minimum);
   
    /*printf("%f, %f, (%d, %d)\n", minimum, maximum, xres, yres);*/
    
    pixels = gdk_pixbuf_get_pixels(g);
    rowstride = gdk_pixbuf_get_rowstride(g);
    
    for (i=0; i<(yres); i++)
    {
	line=pixels + i*rowstride;
    	for (j=0; j<(xres); j++)
	{   
	    dval=(gint)((f->data[i + j*f->yres]-minimum)*kor);
	    rval=pal->r[dval]; gval=pal->g[dval]; bval=pal->b[dval];
	    *(line++) = rval;
	    *(line++) = gval;
	    *(line++) = bval;
	}
    }
}

void gwy_pixfield_do_log(GdkPixbuf *g, GwyDataField *f, PixPalette *pal)
{
    int xres = f->xres;
    int yres = f->yres;
    int i, j;
    guchar *pixels, *line;
    guchar rval, gval, bval;
    gint rowstride, dval;
    gdouble maximum, minimum, kor;

    gwy_datafield_add(f, -gwy_datafield_get_min(f) + 1);
    
    maximum=log(gwy_datafield_get_max(f)); 
    minimum=log(gwy_datafield_get_min(f));
    kor=511.0/(maximum-minimum); 
    
    pixels = gdk_pixbuf_get_pixels(g);
    rowstride = gdk_pixbuf_get_rowstride(g);
    
    for (i=0; i<(yres); i++)
    {
	line=pixels + i*rowstride;
    	for (j=0; j<(xres); j++)
	{   
	    dval=(gint)((log(f->data[i + j*f->yres])-minimum)*kor);
	    rval=pal->r[dval]; gval=pal->g[dval]; bval=pal->b[dval];
	    *(line++) = rval;
	    *(line++) = gval;
	    *(line++) = bval;
	}
    }
}


