/*
 *  @(#) $Id$
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

#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gtk/gtk.h>

#include "libgwyprocess.h"
#include <libdraw/gwypalette.h>
#include <libdraw/gwypixfield.h>

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
	    a->data[i + j*yres] = sin((gdouble)i*j*0.75/xres);
//	    a->data[i + j*yres] = exp(-((((gdouble)i-255.5)*((gdouble)i-255.5)+((gdouble)j-255.5)*((gdouble)j-255.5)))/300);
//	    a->data[i + j*yres] = 10*sin(6.28*(i+j)/50);
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
    gint i, j;
 
    GwyDataField *a, *b, *c, *d, *p;
    GwyDataLine *k;

    GwyPalette *pal;
    
    g_type_init();
  
    g_message("preparing data_field...");
    a = (GwyDataField *) gwy_data_field_new(512, 512, 512, 512, 1);
    b = (GwyDataField *) gwy_data_field_new(512, 512, 512, 512, 1);
    c = (GwyDataField *) gwy_data_field_new(512, 512, 512, 512, 1);
    d = (GwyDataField *) gwy_data_field_new(512, 512, 512, 512, 1);
    p = (GwyDataField *) gwy_data_field_new(512, 512, 512, 512, 1);

    k = (GwyDataLine *) gwy_data_line_new(20, 20, 1);
    make_test_image(a);
    make_test_image(p);

    gwy_data_field_2dfft(a, b, c, d, gwy_data_line_fft_hum, GWY_WINDOWING_RECT, 1, GWY_INTERPOLATION_BILINEAR, 0, 0);
    gwy_data_field_copy(c, a);
    gwy_data_field_copy(d, b);

    gwy_data_field_fill(c, 0);
    gwy_data_field_fill(d, 0);
    gwy_data_field_2dfft(a, b, c, d, gwy_data_line_fft_hum, GWY_WINDOWING_RECT, -1, GWY_INTERPOLATION_BILINEAR, 0, 0);

/*    for (i=0; i<512; i++) 
    {
       printf("%f  %f\n", a->data[20 + 512*i], //sqrt(a->data[20 + 512*i]*a->data[20 + 512*i]+ b->data[20 + 512*i]*b->data[20 + 512*i]), 
	p->data[20 + 512*i]);
    }
*/
/*
    gwy_palette_def_setup_presets();
    pal = (GwyPalette*) gwy_palette_new(NULL);
    gwy_palette_set_by_name(pal, GWY_PALETTE_OLIVE);

    pxb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, a->xres, a->yres);
    gwy_pixfield_do(pxb, a, pal);
    gdk_pixbuf_save(pxb, "a.jpg", "jpeg", &error, "quality", "100", NULL);
    gwy_pixfield_do(pxb, b, pal);
    gdk_pixbuf_save(pxb, "b.jpg", "jpeg", &error, "quality", "100", NULL);

    gwy_pixfield_do(pxb, c, pal);
    gdk_pixbuf_save(pxb, "c.jpg", "jpeg", &error, "quality", "100", NULL);
    gwy_pixfield_do(pxb, d, pal);
    gdk_pixbuf_save(pxb, "d.jpg", "jpeg", &error, "quality", "100", NULL);
*/



   
    return 0;
}
