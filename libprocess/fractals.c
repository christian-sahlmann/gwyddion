/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Jindrich Bilek.
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

#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"


/*
 * @data_field: data field
 * @xresult: x-values for log-log plot
 * @yresult: y-values for log-log plot
 *
 * Computes data for log-log plot by partitioning.
 *
 * Returns two GwyDataLines with same size.
 *
 * Returns: data for log-log plot obtained by partitioning
 */
void
gwy_data_field_fractal_partitioning(GwyDataField *data_field,
                                    GwyDataLine *xresult, GwyDataLine *yresult,
                                    GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, rp, dimexp, xnewres;
    gdouble rms;


    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (gint)pow(2, dimexp) + 1;

    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);

    for (l = 0; l < dimexp; l++) {
        rp = ROUND(pow(2, l));
        for (i = 0; i < ((buffer->xres - 1)/rp - 1); i++) {
            for (j = 0; j < ((buffer->yres - 1)/rp - 1); j++) {
                rms = gwy_data_field_get_area_rms(buffer, i * (rp), j * (rp),
                                                  (i + 1) * (rp) + 1,
                                                  (j + 1) * (rp) + 1);
                yresult->data[l] += rms * rms;
            }
        }
        xresult->data[l] = log(rp);
        yresult->data[l] =
            log(yresult->data[l] /
                (((buffer->xres - 1)/rp - 1) * ((buffer->yres - 1)/rp - 1)));

    }
    g_object_unref(buffer);
}



void
gwy_data_field_fractal_cubecounting(GwyDataField *data_field,
                                    GwyDataLine *xresult, GwyDataLine *yresult,
                                    GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, m, n, rp, rp2, dimexp;

    gdouble a, max, min, imin, hlp, height, xnewres;

    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (gint)pow(2, dimexp) + 1;

    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);

    imin = gwy_data_field_get_min(buffer);
    height = gwy_data_field_get_max(buffer) - imin;

    for (l = 0; l < dimexp; l++) {
        rp = ROUND(pow(2, l + 1));
        rp2 = ROUND(pow(2, dimexp)/rp);
        a = height/rp;
        for (i = 0; i < rp; i++) {
            for (j = 0; j < rp; j++) {
                max = -G_MAXDOUBLE;
                min = G_MAXDOUBLE;
                for (m = 0; m <= rp2; m++) {
                    for (n = 0; n <= rp2; n++) {
                        hlp =
                            buffer->data[(i * rp2 + m) +
                                         buffer->xres * (j * rp2 + n)] - imin;
                        if (hlp > max)
                            max = hlp;
                        if (hlp < min)
                            min = hlp;
                    }
                }
                yresult->data[l] +=
                    rp - floor(min/a) - floor((height - max)/a);
            }
        }
        xresult->data[l] = log(1/pow(2, dimexp - (l + 1)));
    }
    for (l = 0; l < dimexp; l++) {
        yresult->data[l] = log(yresult->data[l]);
    }
    g_object_unref(buffer);
}


void
gwy_data_field_fractal_triangulation(GwyDataField *data_field,
                                     GwyDataLine *xresult, GwyDataLine *yresult,
                                     GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, rp, rp2, dimexp, xnewres;

    gdouble dil, a, b, c, d, e, s1, s2, s, z1, z2, z3, z4, height;

    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (gint)pow(2, dimexp) + 1;

    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp + 1, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp + 1, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);


    height = gwy_data_field_get_max(buffer) - gwy_data_field_get_min(buffer);
    dil = pow(2, dimexp) * pow(2, dimexp)/height/height;

    for (l = 0; l <= dimexp; l++) {
        rp = ROUND(pow(2, l));
        rp2 = ROUND(pow(2, dimexp)/rp);
        for (i = 0; i < rp; i++) {
            for (j = 0; j < rp; j++) {
                z1 = buffer->data[(i * rp2) + buffer->xres * (j * rp2)];
                z2 = buffer->data[((i + 1) * rp2) + buffer->xres * (j * rp2)];
                z3 = buffer->data[(i * rp2) + buffer->xres * ((j + 1) * rp2)];
                z4 = buffer->data[((i + 1) * rp2) +
                                  buffer->xres * ((j + 1) * rp2)];

                a = sqrt(rp2 * rp2 + dil * (z1 - z2) * (z1 - z2));
                b = sqrt(rp2 * rp2 + dil * (z1 - z3) * (z1 - z3));
                c = sqrt(rp2 * rp2 + dil * (z3 - z4) * (z3 - z4));
                d = sqrt(rp2 * rp2 + dil * (z2 - z4) * (z2 - z4));
                e = sqrt(2 * rp2 * rp2 + dil * (z3 - z2) * (z3 - z2));

                s1 = (a + b + e)/2;
                s2 = (c + d + e)/2;
                s = sqrt(s1 * (s1 - a) * (s1 - b) * (s1 - e)) +
                    sqrt(s2 * (s2 - c) * (s2 - d) * (s2 - e));

                yresult->data[l] += s;
            }
        }
        xresult->data[l] = log(1/pow(2, dimexp - l));
    }
    for (l = 0; l <= dimexp; l++) {
        yresult->data[l] = log(yresult->data[l]);
    }
    g_object_unref(buffer);
}

void
gwy_data_field_fractal_psdf(GwyDataField *data_field, GwyDataLine *xresult,
                            GwyDataLine *yresult,
                            GwyInterpolationType interpolation)
{
    gint i;

    gwy_data_line_resample(yresult, data_field->xres, GWY_INTERPOLATION_NONE);

    gwy_data_field_get_line_stat_function(data_field, yresult,
                                          0,
                                          0,
                                          data_field->xres,
                                          data_field->yres,
                                          GWY_SF_OUTPUT_PSDF,
                                          GTK_ORIENTATION_HORIZONTAL,
                                          interpolation,
                                          GWY_WINDOWING_HANN, yresult->res);

    gwy_data_line_resample(xresult, yresult->res, GWY_INTERPOLATION_NONE);

    for (i = 1; i < yresult->res; i++) {
        xresult->data[i - 1] = log(i);
        yresult->data[i - 1] = log(yresult->data[i]);
    }
    gwy_data_line_resize(xresult, 0, yresult->res - 1);
    gwy_data_line_resize(yresult, 0, yresult->res - 1);

}


void
gwy_data_field_fractal_fit(GwyDataLine *xresult, GwyDataLine *yresult,
                           gdouble *a, gdouble *b)
{
    gdouble sx = 0, sxy = 0, sx2 = 0, sy = 0;
    gint i, size;

    size = gwy_data_line_get_res(xresult);
    for (i = 0; i < size; i++) {
        sx += xresult->data[i];
        sx2 += xresult->data[i] * xresult->data[i];
        sy += yresult->data[i];
        sxy += xresult->data[i] * yresult->data[i];
    }
    *a = (sxy - sx * sy/size)/(sx2 - sx * sx/size);
    *b = (sx2 * sy - sx * sxy)/(sx2 * size - sx * sx);
}



gdouble
gwy_data_field_fractal_cubecounting_dim(GwyDataLine *xresult,
                                        GwyDataLine *yresult, gdouble *a,
                                        gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return *a;
}

gdouble
gwy_data_field_fractal_triangulation_dim(GwyDataLine *xresult,
                                         GwyDataLine *yresult, gdouble *a,
                                         gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 2 + (*a);
}

gdouble
gwy_data_field_fractal_partitioning_dim(GwyDataLine *xresult,
                                        GwyDataLine *yresult, gdouble *a,
                                        gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 3 - (*a)/2;
}

gdouble
gwy_data_field_fractal_psdf_dim(GwyDataLine *xresult, GwyDataLine *yresult,
                                gdouble *a, gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 3.5 + (*a)/2;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
