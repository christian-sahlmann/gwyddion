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
#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"
#include "cwt.h"

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"
/*local functions*/
gint step_by_one(GwyDataField *data_field, gint *rcol, gint *rrow);
void threshold_drops(GwyDataField *water_field, gdouble locate_thresh);
void check_neighbours(GwyDataField *data_field, GwyDataField *buffer, gint col, gint row,
                                   gint *global_number, gdouble *global_maximum_value, gint *global_col_value, gint *global_row_value);
void drop_step (GwyDataField *data_field, GwyDataField *water_field, gdouble dropsize);
void drop_minima (GwyDataField *water_field, GwyDataField *min_field, gint threshval);
gint wstep_by_one(GwyDataField *data_field, GwyDataField *grain_field, gint *rcol, gint *rrow, gint last_grain);
void process_mask(GwyDataField *grain_field, gint col, gint row);
void wdrop_step (GwyDataField *data_field,  GwyDataField *min_field,  GwyDataField *water_field, GwyDataField *grain_field, gdouble dropsize);
void mark_grain_boundaries (GwyDataField *grain_field, GwyDataField *mark_field);

/********************/

void gwy_data_field_grains_mark_local_maxima(GwyDataField *data_field, GwyDataField *grain_field)
{
    
}

void gwy_data_field_grains_mark_height(GwyDataField *data_field, GwyDataField *grain_field, gdouble threshval)
{
}

void gwy_data_field_grains_mark_slope(GwyDataField *data_field, GwyDataField *grain_field, gdouble threshval)
{
}

void gwy_data_field_grains_mark_curvature(GwyDataField *data_field, GwyDataField *grain_field, gdouble threshval)
{
}

void gwy_data_field_grains_mark_watershed(GwyDataField *data_field, GwyDataField *grain_field,
					  gint locate_steps, gint locate_thresh, gdouble locate_dropsize,
					  gint wshed_steps, gdouble wshed_dropsize)
{
    GwyDataField *min, *water;
    gint i;

    min = (GwyDataField*)gwy_data_field_new(data_field->xres, data_field->yres, data_field->xreal, data_field->yreal, TRUE);
    water = (GwyDataField*)gwy_data_field_new(data_field->xres, data_field->yres, data_field->xreal, data_field->yreal, TRUE);
    
    /*odrop*/
    for (i=0; i<locate_steps; i++)
    {
	    drop_step(data_field, water, locate_dropsize);
    }
    threshold_drops(water, locate_thresh);

    drop_minima(water, min, locate_thresh);

    /*owatershed*/
    for (i=0; i<wshed_steps; i++)
    {
	    wdrop_step(data_field, min, water, grain_field, wshed_dropsize); 
    }
    
    /*mark_grain_boundaries(water_field, grain_field);*/

    g_object_unref(min);
    g_object_unref(water);

}

void gwy_data_field_grains_remove_manually(GwyDataField *data_field, GwyDataField *grain_field, gint col, gint row)
{
}

void gwy_data_field_grains_remove_by_size(GwyDataField *data_field, GwyDataField *grain_field, gdouble size)
{
}

void gwy_data_field_grains_remove_by_height(GwyDataField *data_field, GwyDataField *grain_field, gdouble threshval, gint direction)
{
}

gdouble gwy_data_field_grains_get_average(GwyDataField *grain_field)
{
}

void gwy_data_field_grains_get_distribution(GwyDataField *grain_field, GwyDataLine *distribution)
{
}


/***********************************************************************************************************************/
/*private functions*/

gint step_by_one(GwyDataField *data_field, gint *rcol, gint *rrow)
{
    gint xres, yres;
    gdouble a, b, c, d, v;
    
    xres = data_field->xres;
    yres = data_field->yres;

    if (*rcol<(xres-1)) a = data_field->data[*rcol+1 + xres*(*rrow)]; else a = -G_MAXDOUBLE;
    if (*rcol>0)        b = data_field->data[*rcol-1 + xres*(*rrow)]; else b = -G_MAXDOUBLE;
    if (*rrow<(yres-1)) c = data_field->data[*rcol + xres*(*rrow+1)]; else c = -G_MAXDOUBLE;
    if (*rrow>0)        d = data_field->data[*rcol + xres*(*rrow-1)]; else d = -G_MAXDOUBLE;
				    
    v = data_field->data[(gint)(rcol + xres*(*rrow))];

    if (v>=a && v>=b && v>=c && v>=d) {return 1;}
    else if (a>=v && a>=b && a>=c && a>=d) {*rcol++; return 0;}
    else if (b>=v && b>=a && b>=c && b>=d) {*rcol--; return 0;}
    else if (c>=v && c>=b && c>=a && c>=d) {*rrow++; return 0;}
    else {*rrow--; return 0;}
    
    return 0;
}

void threshold_drops(GwyDataField *water_field, gdouble locate_thresh)
{
    gint i, xres, yres;
    
    xres = water_field->xres;
    yres = water_field->yres;

    for (i=0; i<(xres*yres); i++) 
    {
        if (water_field->data[i]<=locate_thresh) water_field->data[i]=0;
    }

}

void check_neighbours(GwyDataField *data_field, GwyDataField *buffer, gint col, gint row, 
		     gint *global_number, gdouble *global_maximum_value, gint *global_col_value, gint *global_row_value)
{
    gint xres, yres;
    
    *global_number++;
    xres = data_field->xres;
    yres = data_field->yres;

    if (col<0 || row<0 || col>(xres-1) || row>(yres-1)) return;

    /*mark as judged point and check if this is the minimum*/
    buffer->data[col + xres*row] = 1;
    if (*global_maximum_value < data_field->data[(gint)(col + xres*row)])
    {
	*global_maximum_value = data_field->data[col + xres*row];
	*global_col_value = col;
	*global_row_value = row;
    }

    if (col<(xres-1) && buffer->data[col+1 +xres*row]==0)
	check_neighbours(data_field, buffer, col+1, row, global_number, global_maximum_value, global_col_value, global_row_value);
    if (col>0 && buffer->data[col-1 +xres*row]==0)
	check_neighbours(data_field, buffer, col-1, row, global_number, global_maximum_value, global_col_value, global_row_value);
    if (row<(yres-1) && buffer->data[col + xres*(row+1)]==0)
	check_neighbours(data_field, buffer, col, row+1, global_number, global_maximum_value, global_col_value, global_row_value);
    if (row>0 && buffer->data[col + xres*(row-1)]==0)
	check_neighbours(data_field, buffer, col, row-1, global_number, global_maximum_value, global_col_value, global_row_value);
     
    
}

void drop_step (GwyDataField *data_field, GwyDataField *water_field, gdouble dropsize)
{     
    gint xres, yres, i, retval;
    gint col, row;
    
    xres = data_field->xres;
    yres = data_field->yres;

    for (i=0; i<(xres*yres); i++)
    {
	retval = 0;
	row = (gint)floor((gdouble)i/(gdouble)xres); 
	col = i - row;
	do {
	    retval = step_by_one(data_field, &col, &row);
	} while (retval==0);
	
	water_field->data[i] += 1;
	data_field->data[i] -= dropsize;
	
    }
}

void drop_minima (GwyDataField *water_field, GwyDataField *min_field, gint threshval)
{
    gint xres, yres, i, retval, global_row_value, global_col_value, global_number;
    gdouble col, row, global_maximum_value;
    GwyDataField *buffer;
    
    xres = water_field->xres;
    yres = water_field->yres;
    buffer = (GwyDataField*)gwy_data_field_new(xres, yres, water_field->xreal, water_field->yreal, TRUE);

    for (i=0; i<(xres*yres); i++)
    {
	    if (water_field->data[i]>0 && buffer->data[i]==0)
    	{
    	    global_maximum_value = water_field->data[i];
    	    row = global_row_value = (gint)floor((gdouble)i/(gdouble)xres);
    	    col = global_col_value = i - row;
    	    global_number = 0;
    	    check_neighbours(water_field, buffer, col, row, 
    			     &global_number, &global_maximum_value, &global_col_value, &global_row_value);
    	    
    	    if (global_number>threshval)
           	    {
    	        min_field->data[global_col_value + xres*global_row_value] = global_number;
    	    }
    	}
    }
    
}

gint wstep_by_one(GwyDataField *data_field, GwyDataField *grain_field, gint *rcol, gint *rrow, gint last_grain)
{
    gint xres, yres;
    gdouble a, b, c, d, v;
    
    xres = data_field->xres;
    yres = data_field->yres;

    grain_field->data[*rcol + xres*(*rrow)]=last_grain;

    if (*rcol<(xres-1) && (grain_field->data[*rcol+1 + xres*(*rrow)]==0 || grain_field->data[*rcol+1 + xres*(*rrow)]==last_grain)) 
        a = data_field->data[*rcol+1 + xres*(*rrow)]; 
    else a = -G_MAXDOUBLE;
    
    if (*rcol>0 && (grain_field->data[*rcol-1 + xres*(*rrow)]==0 || grain_field->data[*rcol-1 + xres*(*rrow)]==last_grain))        
        b = data_field->data[*rcol-1 + xres*(*rrow)]; 
    else b = -G_MAXDOUBLE;
    
    if (*rrow<(yres-1) && (grain_field->data[*rcol + xres*(*rrow+1)]==0 || grain_field->data[*rcol + xres*(*rrow+1)]==last_grain)) 
        c = data_field->data[*rcol + xres*(*rrow+1)]; 
    else c = -G_MAXDOUBLE;
    
    if (*rrow>0 && (grain_field->data[*rcol + xres*(*rrow-1)]==0 || grain_field->data[*rcol + xres*(*rrow-1)]==last_grain))        
        d = data_field->data[*rcol + xres*(*rrow-1)]; 
    else d = -G_MAXDOUBLE;
				    
    v = data_field->data[(gint)(rcol + xres*(*rrow))];

    if (v>=a && v>=b && v>=c && v>=d) {return 1;}
    else if (a>=v && a>=b && a>=c && a>=d) {*rcol++; return 0;}
    else if (b>=v && b>=a && b>=c && b>=d) {*rcol--; return 0;}
    else if (c>=v && c>=b && c>=a && c>=d) {*rrow++; return 0;}
    else {*rrow--; return 0;}
    
    
    
}

void process_mask(GwyDataField *grain_field, gint col, gint row)
{
    gint xres, yres, ival[4], val, stat, i;
    
    xres = grain_field->xres;
    yres = grain_field->yres;

    if (col==0 || row==0 || col==(xres-1) || row==(yres-1)) 
    {
        grain_field->data[col + xres*(row)] = -1;
        return;
    }
    
    /*if this is grain or boundary, keep it*/
    if (grain_field->data[col + xres*(row)] != 0) return;
    
    /*if there is nothing around, do nothing*/
    if ((fabs(grain_field->data[col+1 + xres*(row)]) + fabs(grain_field->data[col-1 + xres*(row)]) 
         + fabs(grain_field->data[col + xres*(row+1)]) + fabs(grain_field->data[col + xres*(row-1)]))==0) return;

    /*now count the grain values around*/
    ival[0] = grain_field->data[col-1 + xres*(row)];
    ival[1] = grain_field->data[col + xres*(row-1)];
    ival[2] = grain_field->data[col+1 + xres*(row)];
    ival[3] = grain_field->data[col + xres*(row+1)];

    val = 0;
    stat = 0;
    for (i=0; i<4; i++)
    {
        if (val>0 && ival[i]>0 && ival[i]!=val)
        {
            /*if some value already was there and the now one is different*/
            stat=1; break;
        }
        else
        {
            /*ifthere is some value*/
            if (ival[i]>0) {val=ival[i];}
        }
    }
    
    /*it will be boundary or grain*/
    if (stat==1) grain_field->data[col + xres*(row)] = -1;
    else grain_field->data[col + xres*(row)] = val;
    
}

void wdrop_step (GwyDataField *data_field,  GwyDataField *min_field,  GwyDataField *water_field, GwyDataField *grain_field, gdouble dropsize)
{
    gint xres, yres, vcol, vrow, col, row, grain, retval;
    
    xres = data_field->xres;
    yres = data_field->yres;

    grain = 0;
    for (col=1; col<(xres-1); col++)
    {
        for (row=1; row<(yres-1); row++)
        {
            if (min_field->data[col + xres*(row)]>0 && water_field->data[col + xres*(row)]==0) 
                grain_field->data[col + xres*(row)] = grain++;

            vcol = col; vrow = row;
            retval = 0;
            do {
                retval = step_by_one(data_field, &vcol, &vrow);
            } while (retval==0);

            /*now, distinguish what to change at point vi, vj*/
            process_mask(grain_field, vcol, vrow);
            water_field->data[vcol + xres*(vrow)] += dropsize;
            data_field->data[vcol + xres*(vrow)] -= dropsize;
            
        }
    } 
}

void mark_grain_boundaries (GwyDataField *grain_field, GwyDataField *mark_field)
{
    gint xres, yres, col, row;
    
    xres = grain_field->xres;
    yres = grain_field->yres;

    for (col=1; col<(xres-1); col++)
    {
        for (row=1; row<(yres-1); row++)
        {
            if (mark_field->data[col + xres*(row)] != mark_field->data[col+1 + xres*(row)]
                || mark_field->data[col + xres*(row)] != mark_field->data[col + xres*(row+1)])
                grain_field->data[col + xres*(row)] = 0;
        }
    }
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
