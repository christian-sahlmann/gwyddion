#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>
#include "dataline.h"
#include "datafield.h"
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
    GError *error=NULL;
    guchar *buffer;
    gsize size, pos;
    FILE *fh;
    gint i;
 
    GwyDataField *a, *b;
    GwyDataLine *c, *d;
    
    g_type_init();
  
    g_message("preparing data_field...");
    a = (GwyDataField *) gwy_data_field_new(500, 500, 500, 500, 1);
    make_test_image(a);
    
    c = (GwyDataLine *) gwy_data_line_new(500, 500, 1);
    make_test_line(c);
    /*test anything with the processing routines*/

   

    /*test serialization of the data_field*/
    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize((GObject *)a, buffer, &size);
    buffer = gwy_serializable_serialize((GObject *)c, buffer, &size);
   
    printf("size is %d\n", size);
    g_message("writing data_field and data_line to test.data_field...");
    
    
    fh = fopen("test.data_field", "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_object_unref((GObject *)a);
    g_object_unref((GObject *)c);

    
    g_message("reading data_field and data_line from test.data_field...");
    g_file_get_contents("test.data_field", (gchar**)&buffer, &size, &error);
    pos = 0;
    b = (GwyDataField *) gwy_serializable_deserialize(buffer, size, &pos);
    d = (GwyDataLine *) gwy_serializable_deserialize(buffer, size, &pos);
      

    g_message("outputting data_line to xline.dat...");
    fh = fopen("xline.dat", "w");
    for (i=0; i<d->res; i++) fprintf(fh, "%d  %f\n", i, d->data[i]);
    fclose(fh);
    
    g_object_unref((GObject *)b);
    g_object_unref((GObject *)d);
    return 0;
}
