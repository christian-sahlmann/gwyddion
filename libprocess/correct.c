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



/**
 * gwy_data_field_correct_laplace_iteration:
 * @data_field: data field to be corrected 
 * @mask_field: mask of places to be corrected
 * @buffer_field: initialized to same size aa mask and data
 * @error: maximum change within last step
 * @corfactor: correction factor within step.
 *
 * Tries to remove all the points in mask off the data by using
 * iterative method similar to solving heat flux equation.
 * Use this function repeatedly until reasonable @error is reached. 
 **/
void
gwy_data_field_correct_laplace_iteration(GwyDataField *data_field, GwyDataField *mask_field, GwyDataField *buffer_field, 
                                         gdouble *error, gdouble *corfactor)
{
    gint xres, yres, i, j;
    gdouble cor;

    xres = data_field->xres;
    yres = data_field->yres;
    
    /*check buffer field*/
    if (buffer_field == NULL) 
    {
        buffer_field = (GwyDataField *)gwy_data_field_new(xres, yres, data_field->xreal, data_field->yreal, TRUE);
    }
    if (buffer_field->xres != xres || buffer_field->yres != yres) 
    {
        gwy_data_field_resample(buffer_field, xres, yres, GWY_INTERPOLATION_NONE);
    }
    gwy_data_field_copy(data_field, buffer_field);

    /*set boundary condition for masked boundary data*/
    for (i=0; i<xres; i++)
    {
        if (mask_field->data[i]!=0) buffer_field->data[i] = buffer_field->data[i + 2*xres];
        if (mask_field->data[i + xres*(yres-1)]!=0) buffer_field->data[i + xres*(yres-1)] = buffer_field->data[i + xres*(yres-3)];
    }
    for (i=0; i<yres; i++)
    {
        if (mask_field->data[xres*i]!=0) buffer_field->data[xres*i] =  buffer_field->data[2 + xres*i];
        if (mask_field->data[yres - 1 + xres*i]!=0) buffer_field->data[yres - 1 + xres*i] =  buffer_field->data[yres - 3 + xres*i];
    }

    *error = 0;
    /*iterate*/
    for (i=1; i<(xres-1); i++)
    {
        for (j=1; j<(yres-1); j++)
        {
            if (mask_field->data[i + xres*j]!=0)
            {
                cor = (*corfactor)*(
                                (data_field->data[i+1 + xres*j] + data_field->data[i-1 + xres*j] 
                                 - 2*data_field->data[i + xres*j])
                              + (data_field->data[i + xres*(j+1)] + data_field->data[i + xres*(j-1)] 
                                 - 2*data_field->data[i + xres*j]));

                buffer_field->data[i + xres*j] += cor;
                if (fabs(cor)>(*error)) (*error) = fabs(cor);
            }
        }
    }

    gwy_data_field_copy(buffer_field, data_field); 

}

/**
 * gwy_data_field_mask_outliers:
 * @data_field: data field 
 * @mask_field: mask to be changed 
 * @thresh: threshold value
 *
 * Creates mask of data that are above or below
 * thresh*sigma from average height. Sigma denotes root-mean square deviation
 * of heights. This criterium corresponds
 * to usual Gaussian distribution outliers detection for thresh = 3.
 **/
void
gwy_data_field_mask_outliers(GwyDataField *data_field, GwyDataField *mask_field, gdouble thresh)
{
    gdouble avg;
    gdouble criterium;
    gint i;

    avg = gwy_data_field_get_avg(data_field);
    criterium = gwy_data_field_get_rms(data_field)*thresh;

    for (i=0; i<(data_field->xres*data_field->yres); i++)
    {
        if (fabs(data_field->data[i]-avg)>criterium)
            mask_field->data[i] = 1;
        else
            mask_field->data[i] = 0;
    }

}


void
gwy_data_field_correct_average(GwyDataField *data_field, GwyDataField *mask_field)
{
   gdouble avg;
   gint i;

   avg = gwy_data_field_get_avg(data_field);
   
   for (i=0; i<(data_field->xres*data_field->yres); i++)
   {
       if (mask_field->data[i]) data_field->data[i] = avg;
   }
    
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
