/* @(#) $Id$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#define N 240

int
main(int argc, char *argv[])
{
    FILE *fh;
    GwyDataField *df;
    gdouble *data, *row;
    gdouble x, y;
    gint i, j;
    guchar *buffer = NULL;
    gsize size = 0;

    g_type_init();
    df = gwy_data_field_new(N, N, M_PI, M_PI, FALSE);
    data = df->data;
    for (i = 0; i < N; i++) {
      row = data + N*i;
      y = 2*M_PI*(double)i/(N-1);
      for (j = 0; j < N; j++) {
        x = 2*M_PI*(double)j/(N-1);
        row[j] = sin(x*y + x-3.0*y);
        if (row[j] < 0)
          row[j] = (x+y)/M_PI/M_PI;
      }
    }
    gwy_data_field_set_xreal(df, 4.1e-8);
    gwy_data_field_set_yreal(df, 4.1e-8);
    buffer = gwy_serializable_serialize(G_OBJECT(df), buffer, &size);

    fh = fopen("data_field.object", "wb");
    fwrite(buffer, 1, size, fh);
    fclose(fh);

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

