#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>
#include "gwypalettedef.h"
#include "gwypalette.h"


int main(int argc, char *argv[])
{
    GError *error=NULL;
    guchar *buffer;
    gsize size, pos;
    FILE *fh;
		
    GwyPalette *p;
    GwyPaletteDef *pal, *pap, *paz;
    GwyPaletteDefEntry cval, dval;
    GArray *ble;
    gint n, i;

    g_type_init();

    /****************************************** palette test ********************************************/
    
    p = (GwyPalette*)gwy_palette_new(512);

    g_message("preparing palette...");
   
    paz = (GwyPaletteDef*)gwy_palette_def_new(512);
    cval.color.r=0; cval.color.g=255; cval.color.b=255; cval.x=0;
    gwy_palette_def_set_color(paz, &cval);
    cval.color.r=255; cval.color.g=255; cval.color.b=255; cval.x=512;
    gwy_palette_def_set_color(paz, &cval);
    
    gwy_palette_def_print(paz);
    gwy_palette_set_def(p, paz);
    gwy_palette_recompute_table(p);
   
    gwy_palette_print(p);

    g_object_unref((GObject *)p);
    return 0;
    /******************************************* palettedef test ****************************************/
    pal = (GwyPaletteDef*)gwy_palette_def_new(512);
 
    g_message("preparing palette...");

    cval.color.r=0; cval.color.g=255; cval.color.b=255;
    cval.x=0;
    
    gwy_palette_def_set_color(pal, &cval);
    
    cval.x=10; cval.color.r=23;
    gwy_palette_def_set_color(pal, &cval);
    
    dval.color.r=23; dval.color.g=33; dval.color.b=43;
    dval.x=85;
    gwy_palette_def_set_color(pal, &dval);
    
    gwy_palette_def_print(pal);

    printf("r: %f %f %f %f %f\n ", (gwy_palette_def_get_color(pal, 1, GWY_INTERPOLATION_BILINEAR)).r,
	  (gwy_palette_def_get_color(pal, 3, GWY_INTERPOLATION_BILINEAR)).r,
	  (gwy_palette_def_get_color(pal, 9, GWY_INTERPOLATION_BILINEAR)).r,
	  (gwy_palette_def_get_color(pal, 12, GWY_INTERPOLATION_BILINEAR)).r,
	  (gwy_palette_def_get_color(pal, 15, GWY_INTERPOLATION_BILINEAR)).r
	   );
    
    g_message("serializing palette...");

    size = 0;
    buffer = NULL;
    buffer = gwy_serializable_serialize((GObject *)pal, buffer, &size);
    fh = fopen("test.palettedef", "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);
    g_object_unref((GObject *)pal);
    
    g_message("deserializing palette...");
    
    g_file_get_contents("test.palettedef", (gchar**)&buffer, &size, &error);
    pos = 0;

    pap = (GwyPaletteDef*) gwy_serializable_deserialize(buffer, size, &pos);
    gwy_palette_def_print(pap);
    
    g_object_unref((GObject *)pap);
    return 0;
}
