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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#define N 240
#define R 20

int
main(void)
{
    static const gchar *magic_header = "GWYO";
    FILE *fh;
    GwyContainer *container;
    GwyDataField *df, *kernel;
    gdouble *data, *row;
    gdouble x, y, r, phi;
    gdouble r1, r2, r3;
    gint i, j;
    GByteArray *buffer;

    g_type_init();
    srand(42);
    container = (GwyContainer*)gwy_container_new();
    df = gwy_data_field_new(N, N, 4.1e-8, 4.1e-8, FALSE);
    kernel = gwy_data_field_new(R, R, 1, 1, FALSE);
    r1 = rand()/(gdouble)RAND_MAX;
    r2 = rand()/(gdouble)RAND_MAX;
    r3 = rand()/(gdouble)RAND_MAX;
    data = kernel->data;
    for (i = 0; i < R; i++) {
        row = data + R*i;
        y = (i - (R-1.0)/2)/(R-1.0);
        for (j = 0; j < R; j++) {
            x = (j - (R-1.0)/2)/(R-1.0);
            r = 2*hypot(x, y);
            phi = atan2(y, x);
            row[j] = MAX(1.0 - r, 0)/60.0
                     *(3.0 + r1*sin(phi))
                     *(4.0 + r2*cos(2*phi))
                     *(5.0 + r3*sin(3*phi + 0.11))
                     + cos(1.5*r);
        }
    }
    r1 = rand()/(gdouble)RAND_MAX;
    r2 = rand()/(gdouble)RAND_MAX;
    r3 = rand()/(gdouble)RAND_MAX;
    data = df->data;
    for (i = 0; i < N; i++) {
        row = data + N*i;
        y = (i - (R-1.0)/2)/(R-1.0);
        for (j = 0; j < N; j++) {
            x = (j - (R-1.0)/2)/(R-1.0);
            row[j] = rand()/(gdouble)RAND_MAX + 0.02*sin(hypot(x+2,y+3));
            if (rand() % 240 + 3*sin(r2*x + r1*y*y*2.0) <= 0)
                row[j] += rand() % 6 + 1;
        }
    }
    gwy_data_field_multiply(df, 12e-12);
    gwy_data_field_area_convolve(df, kernel, 0, 0, N, N);
    gwy_data_field_resample(kernel, 4, 4, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_area_convolve(df, kernel, 0, 0, N, N);
    gwy_container_set_object_by_name(container, "/0/data", G_OBJECT(df));
    /*
       df = gwy_data_field_new(N, N, 4.1e-8, 4.1e-8, FALSE);
       data = df->data;
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
     */
    gwy_container_set_object_by_name(container, "/0/mask", G_OBJECT(df));
    gwy_container_set_double_by_name(container, "/0/mask/red", 1.0);
    gwy_container_set_double_by_name(container, "/0/mask/blue", 0.1);
    gwy_container_set_double_by_name(container, "/0/mask/green", 0.3);
    gwy_container_set_double_by_name(container, "/0/mask/alpha", 1.0);
    gwy_container_set_string_by_name(container, "/meta/Created by",
        g_strdup("$Id$"));
    gwy_container_set_string_by_name(container, "/meta/bracket-test",
                                     g_strdup("["));
    buffer = gwy_serializable_serialize(G_OBJECT(container), NULL);

    fh = fopen("bracket.gwy", "wb");
    fwrite(magic_header, 1, strlen(magic_header), fh);
    fwrite(buffer->data, 1, buffer->len, fh);
    fclose(fh);

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

