/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>
#include "gwypixfield.h"


void
gwy_pixfield_do(GdkPixbuf *g, GwyDataField *f, GwyPalette *pal)
{
    int xres = f->xres;
    int yres = f->yres;
    int i, j, palsize;
    guchar *pixels, *line, *samples, *s;
    gint rowstride, dval;
    gdouble maximum, minimum, cor;

    maximum = gwy_data_field_get_max(f);
    minimum = gwy_data_field_get_min(f);
    cor = (pal->nofvals-1)/(maximum-minimum);

    pixels = gdk_pixbuf_get_pixels(g);
    rowstride = gdk_pixbuf_get_rowstride(g);
    samples = gwy_palette_get_samples(pal, &palsize);

    for (i = 0; i < yres; i++) {
        line=pixels + i*rowstride;
        for (j = 0; j < xres; j++) {
            dval=(gint)((f->data[i + j*f->yres]-minimum)*cor + 0.5);
            /* simply index to the guchar samples, it's faster and no one
             * can tell the difference... */
            s = samples + 4*dval;
            *(line++) = *(s++);
            *(line++) = *(s++);
            *(line++) = *s;

            /*
            *(line++) = (guchar)(gint)(255.999*pal->color[dval].r);
            *(line++) = (guchar)(gint)(255.999*pal->color[dval].g);
            *(line++) = (guchar)(gint)(255.999*pal->color[dval].b);
            */
        }
    }
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
