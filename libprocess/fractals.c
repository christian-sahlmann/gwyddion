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

#include <stdio.h>
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
* Computes data for log-log plot by partitioning. Returns two GwyDataLines with same size.
*
* Returns: data for log-log plot obtained by partitioning
*/
void 
gwy_data_field_fractal_partitioning(GwyDataField *data_field, GwyDataLine *xresult, GwyDataLine *yresult, GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, m, n, rp, dimexp, xnewres;
    gdouble rms;
    
   
    dimexp = (gint) floor(log ((gdouble)data_field->xres)/log (2.0)+0.5);
    xnewres = (gint) pow(2, dimexp)+1;
     
    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);
           
    for(l=0; l<dimexp; l++)
    {
        rp = ROUND(pow(2,l+1));
        for (i=0; i<((buffer->xres-1)/rp); i++)            //        for(i=(ROUND(pow(2,dimexp)/pow(2,l+1))-1); i>=0; i--)
        {
            for (j=0; j<((buffer->yres-1)/rp); j++)                       // for (j=(ROUND(pow(2,dimexp)/pow(2,l+1))-1); j>=0; j--)
            {
                rms = gwy_data_field_get_area_rms(buffer, i*rp, j*rp, (i+1)*rp, (j+1)*rp);
                yresult->data[l] += rms*rms;
            }
        }
        xresult->data[l] = pow(2,l);
        yresult->data[l] /= (pow(2,dimexp)*pow(2,dimexp))/(pow(2,l)*pow(2,l));

    }
    g_object_unref(buffer);
}



void 
gwy_data_field_fractal_cubecounting(GwyDataField *data_field, GwyDataLine *xresult, GwyDataLine *yresult, GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, m, n, rp, rp2, count, dimexp;

    gdouble a, max, min, imin, hlp, height, xorder, xnewres, yorder, ynewres;

    dimexp = (gint) floor(log ((gdouble)data_field->xres)/log (2.0)+0.5);
    xnewres = (gint) pow(2, dimexp)+1;
     
    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);

    imin = gwy_data_field_get_min(buffer);
    height = gwy_data_field_get_max(buffer) - imin;

    for(l=0; l<dimexp; l++)
    {
        rp = ROUND(pow(2,l+1));
        rp2 = ROUND(pow(2,dimexp)/rp);
        a = height/rp;
        for(i=0; i<rp; i++)
        {
            for(j=0; j<rp; j++)
            {
                max = -G_MAXDOUBLE;
                min = G_MAXDOUBLE;
                for(m=0; m<=rp2; m++)
                {
                    for(n=0; n<=rp2; n++)
                    {
                        hlp = buffer->data[(i*rp2+m) + buffer->xres*(j*rp2+n)] - imin;
                        if (hlp > max) max=hlp;
                        if (hlp < min) min=hlp;
                    }
                }
                yresult->data[l] += rp - floor(min/a) - floor((height-max)/a);
            }
        }
        xresult->data[l] = 1/pow(2,dimexp-(l+1));		
    }
    g_object_unref(buffer);
}


void 
gwy_data_field_fractal_triangulation(GwyDataField *data_field, GwyDataLine *xresult, GwyDataLine *yresult, GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, rp, rp2, dimexp, xorder, xnewres, yorder, ynewres;

    gdouble dil, a, b, c, d, e, s1, s2, s, z1, z2, z3, z4, height;

    dimexp = (gint) floor(log ((gdouble)data_field->xres)/log (2.0)+0.5);
    xnewres = (gint) pow(2, dimexp)+1;
     
    buffer = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(data_field)));
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(yresult, 0);


    height = gwy_data_field_get_max(buffer) - gwy_data_field_get_min(buffer);
    dil = pow(2,dimexp) * pow(2,dimexp) / height / height;    
    
    for(l=0; l<dimexp; l++)
    {
        rp = ROUND(pow(2,l));
        rp2 = ROUND(pow(2,dimexp)/rp);
        for(i=0; i<rp; i++)
        {
            for(j=0; j<rp; j++)
            {
                z1 = buffer->data[(i*rp2) + buffer->xres*(j*rp2)];
                z2 = buffer->data[((i+1)*rp2) + buffer->xres*(j*rp2)];
                z3 = buffer->data[(i*rp2) + buffer->xres*((j+1)*rp2)];
                z4 = buffer->data[((i+1)*rp2) + buffer->xres*((j+1)*rp2)];

                a = sqrt(rp2*rp2+dil*(z1-z2)*(z1-z2));
                b = sqrt(rp2*rp2+dil*(z1-z3)*(z1-z3));
                c = sqrt(rp2*rp2+dil*(z3-z4)*(z3-z4));
                d = sqrt(rp2*rp2+dil*(z2-z4)*(z2-z4));
                e = sqrt(2*rp2*rp2+dil*(z3-z2)*(z3-z2));
			
                s1 = (a+b+e) / 2;
                s2 = (c+d+e) / 2;
                s = sqrt(s1*(s1-a)*(s1-b)*(s1-e))+sqrt(s2*(s2-c)*(s2-d)*(s2-e));
				
				yresult->data[l] += s;
            }
        }
        xresult->data[l] = 1.0/(gdouble)rp2;
    }
    g_object_unref(buffer);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
