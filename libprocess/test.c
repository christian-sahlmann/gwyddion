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
    guchar *buffer;
    gsize size, pos;
    FILE *fh;
 
    GwyDataField *a, *b;
    
    g_type_init();
  
    g_message("preparing datafield...");
    a = (GwyDataField *) gwy_datafield_new(500, 500, 500, 500, 1);
    make_test_image(a);
  
    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize((GObject *)a, buffer, &size);
    
    g_message("writing datafield to test.datafield...");
    fh = fopen("test.datafield", "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    gwy_datafield_free(a);
    g_object_unref((GObject *)a);
    
    g_message("reading datafield from test.datafield...");
    g_file_get_contents("test.datafield", (gchar**)&buffer, &size, &error);
    pos = 0;
    b = (GwyDataField *) gwy_serializable_deserialize(buffer, size, &pos);
    
    g_message("drawing datafield...");
    gwy_pixfield_presetpal(&pal, GWY_PAL_OLIVE);
   
    pxb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 
			 gwy_datafield_get_xres(a), gwy_datafield_get_yres(a));
    gwy_pixfield_do(pxb, a, &pal); 
    gdk_pixbuf_save(pxb, "xout.jpg", "jpeg", &error, "quality", "100", NULL);
    
    gwy_datafield_free(b); 
    return 0;
}
