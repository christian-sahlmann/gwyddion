/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include "gwypalettedef.h"
#include "gwypalette.h"
#include "gwypixfield.h"
#include <libprocess/datafield.h>

int main(void)
{
    GError *error=NULL;
    guchar *buffer;
    gsize size, pos;
    FILE *fh;

    GwyPalette *p, *r;

    GwyDataField *a;
    GdkPixbuf *pxb;

    gint i, j;

    g_type_init();

    /****************************************** palette test ********************************************/

    g_message("preparing palette...");
    gwy_palette_def_setup_presets();
    p = (GwyPalette*)gwy_palette_new(NULL);
    gwy_palette_set_by_name(p, GWY_PALETTE_RAINBOW2);

    /*gwy_palette_def_print(p->def);*/

    buffer = (guchar*)gwy_palette_get_samples(p, &i);
    printf("%d %d %d %d\n",
           (gint32)256*buffer[40], (gint32)256*buffer[41],
           (gint32)256*buffer[42], (gint32)256*buffer[43]);

    /*plot a field in the given palette*/
    a = (GwyDataField *) gwy_data_field_new(500, 250, 500, 500, 1);
    for (i=0; i<a->yres; i++)
    {
	for (j=0; j<a->xres; j++)
	{
	    a->data[i + j*a->yres] = sin((gdouble)i*i*0.08/a->xres) + cos((gdouble)j*j*0.07/a->xres);
	}
    }
    pxb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                  gwy_data_field_get_xres(a), gwy_data_field_get_yres(a));

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
    gwy_pixfield_do(pxb, a, r);
    gdk_pixbuf_save(pxb, "xout2.jpg", "jpeg", &error, "quality", "100", NULL);

    g_object_unref((GObject *)r);
    return 0;
}
