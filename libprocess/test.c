#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>
#include "dataline.h"
#include "datafield.h"
#include "pixfield.h"
#include "simplefft.h"

void make_test_image(GwyDataField *a)
{
    gdouble val;
    gint i, j, xres, yres;

    xres = a->xres;
    yres = a->yres;

    for (i=0; i<yres; i++)
    {
	for (j=0; j<xres; j++)
	{
	    a->data[i + j*yres] = exp(-((((gdouble)i-255.5)*((gdouble)i-255.5)+((gdouble)j-255.5)*((gdouble)j-255.5)))/300);
	}
    }
}

void make_test_line(GwyDataLine *a)
{
    gint i;
    for (i=0; i<a->res; i++)
    {
    	a->data[i]=sin((gdouble)i*5000*0.01258/a->res);
    }
}

int main(int argc, char *argv[])
{
    GdkPixbuf *pxb;
    PixPalette pal;
    GError *error=NULL;
    GObject *a;
    
    g_type_init();
  
   
    a = gwy_datafield_new(500, 500, 500, 500, 1);
  
    return 0;
    gwy_pixfield_presetpal(&pal, GWY_PAL_OLIVE);
    
    pxb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 
			 gwy_datafield_get_xres((GwyDataField *)a), gwy_datafield_get_yres((GwyDataField *)a));
    gwy_pixfield_do(pxb, (GwyDataField *)a, &pal); 
    gdk_pixbuf_save(pxb, "xout.jpg", "jpeg", &error, "quality", "100", NULL);
 
    gwy_datafield_free((GwyDataField *)a); 
    return 0;
}
