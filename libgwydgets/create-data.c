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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#define N 240

int
main(void)
{
    static const gchar *magic_header = "GWYO";
    FILE *fh;
    GwyContainer *container;
    GwyDataField *df;
    gdouble *data, *row;
    gdouble x, y;
    gint i, j;
    guchar *buffer = NULL;
    gsize size = 0;

    g_type_init();
    df = (GwyDataField*)gwy_data_field_new(N, N, 4.1e-8, 4.1e-8, FALSE);
    data = df->data;
    for (i = 0; i < N; i++) {
      row = data + N*i;
      y = 2*G_PI*(double)i/(N-1);
      for (j = 0; j < N; j++) {
        x = 2*G_PI*(double)j/(N-1);
        row[j] = (3*(i/(N/3)) + j/(N/2))*1e-9;
      }
    }
    container = (GwyContainer*)gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", G_OBJECT(df));
    df = (GwyDataField*)gwy_data_field_new(N, N, 4.1e-8, 4.1e-8, FALSE);
    for (i = 0; i < N; i++) {
      row = data + N*i;
      y = 2*G_PI*(double)i/(N-1);
      for (j = 0; j < N; j++) {
        x = 2*G_PI*(double)j/(N-1);
        row[j] = sin(x*y + x-3.0*y);
        if (row[j] < 0)
          row[j] = (x+y)/G_PI/G_PI;
        row[j] = CLAMP(row[j], 0.0, 1.0);
      }
    }
    gwy_container_set_object_by_name(container, "/0/mask", G_OBJECT(df));
    gwy_container_set_string_by_name(container, "/meta/Created by",
                                     "$Id$");
    buffer = gwy_serializable_serialize(G_OBJECT(container), buffer, &size);

    fh = fopen("test.gwy", "wb");
    fwrite(magic_header, 1, strlen(magic_header), fh);
    fwrite(buffer, 1, size, fh);
    fclose(fh);

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

