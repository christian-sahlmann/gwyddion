#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>
#include "gwypalettedef.h"
#include "gwypalette.h"
#include "gwypixfield.h"
#include "datafield.h"

int main(int argc, char *argv[])
{
    GError *error=NULL;
    guchar *buffer;
    gsize size, pos;
    FILE *fh;
		
    GwyPalette *p, *r;
    GwyPaletteEntry pec;

    GwyDataField *a;
    GdkPixbuf *pxb;
    
    gint i, j;

    g_type_init();

    /****************************************** palette test ********************************************/
    
    p = (GwyPalette*)gwy_palette_new(512);

    g_message("preparing palette...");
  
    gwy_palette_setup_predef(p, GWY_PALETTE_BW2);
  
    /*gwy_palette_def_print(p->def);*/
   
    /*
    pec.r=10, pec.g=20; pec.b=30; pec.a=0;
    gwy_palette_set_color(p, &pec, 500);
    */
    printf("%d %d %d %d\n", (gint32)p->ints[40], (gint32)p->ints[41], (gint32)p->ints[42], (gint32)p->ints[43]);

    /*plot a field in the given palette*/
    a = (GwyDataField *) gwy_datafield_new(500, 250, 500, 500, 1);
    for (i=0; i<a->yres; i++)
    {
	for (j=0; j<a->xres; j++)
	{
	    a->data[i + j*a->yres] = sin((gdouble)i*i*0.08/a->xres) + cos((gdouble)j*j*0.07/a->xres);
	}
    }
    pxb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                  gwy_datafield_get_xres(a), gwy_datafield_get_yres(a));
 
    gwy_pixfield_do(pxb, a, p);
    gdk_pixbuf_save(pxb, "xout.jpg", "jpeg", &error, "quality", "100", NULL);

    /*try serialization and deserialization*/
    g_message("serializing palette...");

    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize((GObject *)p, buffer, &size);
    fh = fopen("test.palettedef", "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_object_unref((GObject *)p);
    
    g_message("deserializing palette...");
    
    g_file_get_contents("test.palettedef", (gchar**)&buffer, &size, &error);
    pos = 0;

    r = (GwyPalette*) gwy_serializable_deserialize(buffer, size, &pos);
    /*gwy_palette_print(r);*/
    
    g_object_unref((GObject *)r);
    return 0;
}
